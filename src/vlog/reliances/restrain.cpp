#include "vlog/reliances/reliances.h"

#include <vector>
#include <utility>
#include <list>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>

std::chrono::system_clock::time_point globalRestraintTimepointStart;
unsigned globalRestraintTimeout = 0;
unsigned globalRestraintTimeoutCheckCount = 0;
bool globalRestraintIsTimeout = false;

bool restrainExtend(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo, 
    const VariableAssignments &assignments,
    RelianceStrategy strat);

bool checkUnmappedExistentialVariables(const std::vector<Literal> &literals, 
    const VariableAssignments &assignments)
{
    for (const Literal &literal : literals)
    {
        for (unsigned termIndex = 0; termIndex < literal.getTupleSize(); ++termIndex)
        {
            VTerm currentTerm = literal.getTermAtPos(termIndex);

            if ((int32_t)currentTerm.getId() < 0)
            {
                // if (assignments.getGroupId((int32_t)currentTerm.getId(), RelianceRuleRelation::To) != NOT_ASSIGNED
                //     && assignments.getConstant((int32_t)currentTerm.getId(), RelianceRuleRelation::To) != NOT_ASSIGNED)
                //     return false;

                int64_t assignedConstant = assignments.getConstant((int32_t)currentTerm.getId(), RelianceRuleRelation::To);
                if (assignedConstant != NOT_ASSIGNED && assignedConstant < 0)
                    return false;
            }                
        }
    }

    return true;
}

bool restrainCheckNullsInBody(const std::vector<Literal> &literals,
    const VariableAssignments &assignments, RelianceRuleRelation relation)
{
    for (const Literal &literal : literals)
    {
        for (unsigned termIndex = 0; termIndex < literal.getTupleSize(); ++termIndex)
        {
            VTerm currentTerm = literal.getTermAtPos(termIndex);
        
            if ((int32_t)currentTerm.getId() > 0)
            {
                if (assignments.getConstant((int32_t)currentTerm.getId(), relation) < 0)
                    return false;
            }
        }
    }   

    return true;   
}

bool restrainCheck(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo,
    const VariableAssignments &assignments,
    RelianceStrategy strat)
{
    unsigned nextInDomainIndex = 0;
    const std::vector<Literal> toHeadLiterals = ruleTo.getHeads();
    std::vector<Literal> notMappedHeadLiterals;
    notMappedHeadLiterals.reserve(ruleTo.getHeads().size());
    for (unsigned headIndex = 0; headIndex < toHeadLiterals.size(); ++headIndex)
    {
        if (headIndex == mappingDomain[nextInDomainIndex])
        {
            if (nextInDomainIndex < mappingDomain.size() - 1)
            {
                ++nextInDomainIndex;
            }

            continue;
        }

        notMappedHeadLiterals.push_back(toHeadLiterals[headIndex]);
    }

    if (!restrainCheckNullsInBody(ruleFrom.getBody(), assignments, RelianceRuleRelation::From)
            || !restrainCheckNullsInBody(ruleTo.getBody(), assignments, RelianceRuleRelation::To))
    {
        if ((strat & RelianceStrategy::EarlyTermination) > 0)
            return false;
        else
            return restrainExtend(mappingDomain, ruleFrom, ruleTo, assignments, strat);
    }


    if (!assignments.hasMappedExistentialVariable)
        return restrainExtend(mappingDomain, ruleFrom, ruleTo, assignments, strat);

    if (!checkUnmappedExistentialVariables(notMappedHeadLiterals, assignments))
    {
        return restrainExtend(mappingDomain, ruleFrom, ruleTo, assignments, strat);
    }

    std::vector<std::vector<std::unordered_map<int64_t, TermInfo>>> existentialMappings;
    std::vector<unsigned> satisfied;
    satisfied.resize(ruleTo.getHeads().size(), 0);
    prepareExistentialMappings(ruleTo.getHeads(), RelianceRuleRelation::To, assignments, existentialMappings);

    bool toHeadSatisfied =
        relianceModels(ruleTo.getBody(), RelianceRuleRelation::To, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings);
            
    if (!toHeadSatisfied && ruleTo.isExistential())
    {
        toHeadSatisfied = checkConsistentExistential(existentialMappings);
    }

    if (toHeadSatisfied)
    {
        if ((strat & RelianceStrategy::EarlyTermination) > 0)
            return false;
        else
            return restrainExtend(mappingDomain, ruleFrom, ruleTo, assignments, strat);
    }

    prepareExistentialMappings(ruleTo.getHeads(), RelianceRuleRelation::To, assignments, existentialMappings);
    satisfied.clear();
    satisfied.resize(ruleTo.getHeads().size(), 0);

    bool alternativeMatchAlreadyPresent = relianceModels(ruleTo.getBody(), RelianceRuleRelation::To, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings, false, false);
    alternativeMatchAlreadyPresent |= relianceModels(ruleTo.getHeads(), RelianceRuleRelation::To, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings, true, false);
    alternativeMatchAlreadyPresent |= relianceModels(ruleFrom.getBody(), RelianceRuleRelation::From, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings, false, false);
    alternativeMatchAlreadyPresent |= relianceModels(notMappedHeadLiterals, RelianceRuleRelation::To, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings, false, false);

    if (!alternativeMatchAlreadyPresent && ruleTo.isExistential())
    {
        alternativeMatchAlreadyPresent = checkConsistentExistential(existentialMappings);
    }

    if (alternativeMatchAlreadyPresent)
        return restrainExtend(mappingDomain, ruleFrom, ruleTo, assignments, strat);

    prepareExistentialMappings(ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, existentialMappings);
    satisfied.clear();
    satisfied.resize(ruleFrom.getHeads().size(), 0);

    bool fromHeadSatisfied = relianceModels(ruleTo.getBody(), RelianceRuleRelation::To, ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings);
    fromHeadSatisfied |= relianceModels(ruleTo.getHeads(), RelianceRuleRelation::To, ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings, true);
    fromHeadSatisfied |= relianceModels(ruleFrom.getBody(), RelianceRuleRelation::From, ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings);
    fromHeadSatisfied |= relianceModels(notMappedHeadLiterals, RelianceRuleRelation::To, ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings);

    if (!fromHeadSatisfied && ruleFrom.isExistential())
    {
        fromHeadSatisfied = checkConsistentExistential(existentialMappings);
    }

    if (fromHeadSatisfied)
        return restrainExtend(mappingDomain, ruleFrom, ruleTo, assignments, strat);

    return true;
}

bool restrainExtendAssignment(const Literal &literalFrom, const Literal &literalTo,
    VariableAssignments &assignments)
{
    unsigned tupleSize = literalFrom.getTupleSize(); //Should be the same as literalTo.getTupleSize()

    for (unsigned termIndex = 0; termIndex < tupleSize; ++termIndex)
    {
        VTerm fromTerm = literalFrom.getTermAtPos(termIndex);
        VTerm toTerm = literalTo.getTermAtPos(termIndex);
    
        TermInfo fromInfo = getTermInfoUnify(fromTerm, assignments, RelianceRuleRelation::From);
        TermInfo toInfo = getTermInfoUnify(toTerm, assignments, RelianceRuleRelation::To);

         // We may not assign any universal variable to a null
        if ((toInfo.type == TermInfo::Types::Universal || fromInfo.type == TermInfo::Types::Universal) 
            && (fromInfo.constant < 0 || toInfo.constant < 0))
            return false;

        if (!unifyTerms(fromInfo, toInfo, assignments))
            return false;

        if (toInfo.type == TermInfo::Types::Existential)
            assignments.hasMappedExistentialVariable = true;
    }

    assignments.finishGroupAssignments();

    return true;
}

bool restrainExtend(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo,
    const VariableAssignments &assignments, RelianceStrategy strat)
{
    if (globalRestraintTimeout != 0 && globalRestraintTimeoutCheckCount % 1000 == 0)
    {
        unsigned currentTimeMilliSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - globalRestraintTimepointStart).count();
        if (currentTimeMilliSeconds > globalRestraintTimeout)
        {
          globalRestraintIsTimeout = true;
          return true;
        }
    }

    ++globalRestraintTimeoutCheckCount;

    unsigned headToStartIndex = (mappingDomain.size() == 0) ? 0 : mappingDomain.back() + 1;

    for (unsigned headToIndex = headToStartIndex; headToIndex < ruleTo.getHeads().size(); ++headToIndex)
    {
        const Literal &literalTo = ruleTo.getHeads()[headToIndex];
        mappingDomain.push_back(headToIndex);

        for (unsigned headFromIndex = 0; headFromIndex < ruleFrom.getHeads().size(); ++headFromIndex)
        {
            const Literal &literalFrom =  ruleFrom.getHeads().at(headFromIndex);

            if (literalTo.getPredicate().getId() != literalFrom.getPredicate().getId())
                continue;

            VariableAssignments extendedAssignments = assignments;
            if (!restrainExtendAssignment(literalFrom, literalTo, extendedAssignments))
                continue;

            if (restrainCheck(mappingDomain, ruleFrom, ruleTo, extendedAssignments, strat))
                return true;
        }

        mappingDomain.pop_back();
    }

    return false;
}

bool restrainReliance(const Rule &ruleFrom, unsigned variableCountFrom, const Rule &ruleTo, unsigned variableCountTo, RelianceStrategy strat)
{
    std::vector<unsigned> mappingDomain;
    VariableAssignments assignments(variableCountFrom, variableCountTo);

    return restrainExtend(mappingDomain, ruleFrom, ruleTo, assignments, strat);
}


RelianceComputationResult computeRestrainReliances(const std::vector<Rule> &rules, RelianceStrategy strat, unsigned timeoutMilliSeconds)
{
    RelianceComputationResult result;
    result.graphs = std::pair<SimpleGraph, SimpleGraph>(SimpleGraph(rules.size()), SimpleGraph(rules.size()));
    
    std::vector<Rule> markedRules;
    markedRules.reserve(rules.size());
    
    std::vector<unsigned> variableCounts;
    variableCounts.reserve(rules.size());

    for (size_t ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex)
    {
        const Rule &currentRule = rules[ruleIndex];

        unsigned variableCount = std::max(highestLiteralsId(currentRule.getHeads()), highestLiteralsId(currentRule.getBody()));
        variableCounts.push_back(variableCount + 1);
    }

    for (const Rule &currentRule : rules)
    {
        Rule markedRule = markExistentialVariables(currentRule);
        markedRules.push_back(markedRule);
    }

    std::vector<RuleHashInfo> ruleHashInfos; ruleHashInfos.reserve(rules.size());
    for (const Rule &currentRule : rules)
    {
        ruleHashInfos.push_back(ruleHashInfoFirst(currentRule));
    }

    std::unordered_map<PredId_t, std::vector<size_t>> headToMap;
    std::unordered_map<std::string, bool> resultCache;

    for (size_t ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex)
    {   
        const Rule &markedRule = markedRules[ruleIndex];

        for (const Literal &currentLiteral : markedRule.getHeads())
        {
            bool containsExistentialVariable = false;
            for (size_t termIndex = 0; termIndex < currentLiteral.getTupleSize(); ++termIndex)
            {
                VTerm currentTerm = currentLiteral.getTermAtPos(termIndex);

                if ((int32_t)currentTerm.getId() < 0)
                {
                    containsExistentialVariable = true;
                    break;
                }
            }

            std::vector<size_t> &headToVector = headToMap[currentLiteral.getPredicate().getId()];
            if (containsExistentialVariable)
                headToVector.push_back(ruleIndex);
        }
    }

    std::chrono::system_clock::time_point timepointStart = std::chrono::system_clock::now();
    globalRestraintTimepointStart = timepointStart;
    globalRestraintTimeout = timeoutMilliSeconds;

    uint64_t numCalls = 0;

    enum class RelianceExecutionCommand {
        None,
        Continue,
        Return
    };

    auto relianceExecution = [&] (size_t ruleFrom, size_t ruleTo, std::unordered_set<size_t> &proccesedRules) -> RelianceExecutionCommand {
        if ((strat & RelianceStrategy::CutPairs) > 0)
        {
            if (proccesedRules.find(ruleTo) != proccesedRules.end())
                return RelianceExecutionCommand::Continue;

            proccesedRules.insert(ruleTo);
        }

        if (globalRestraintTimeout != 0 && globalRestraintTimeoutCheckCount % 1000 == 0)
        {
            unsigned currentTimeMilliSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - globalRestraintTimepointStart).count();
            if (currentTimeMilliSeconds > globalRestraintTimeout)
            {
                globalRestraintIsTimeout = true;
                result.timeout = true;
                result.numberOfCalls = numCalls;
                return RelianceExecutionCommand::Return;
            }
        }
        ++globalRestraintTimeoutCheckCount;

        std::string stringHash;
        if ((strat & RelianceStrategy::PairHash) > 0)
        {
            stringHash = rulePairHash(ruleHashInfos[ruleFrom], ruleHashInfos[ruleTo], rules[ruleTo]);
        
            auto cacheIterator = resultCache.find(stringHash);
            if (cacheIterator != resultCache.end())
            {
                if (cacheIterator->second)
                {
                    result.graphs.first.addEdge(ruleFrom, ruleTo);
                    result.graphs.second.addEdge(ruleTo, ruleFrom);
                }

                return RelianceExecutionCommand::Continue;
            }
        }

        ++numCalls;

        unsigned variableCountFrom = variableCounts[ruleFrom];
        unsigned variableCountTo = variableCounts[ruleTo];

        if (ruleFrom == ruleTo)
        {
            std::vector<Rule> splitRules;
            splitIntoPieces(rules[ruleFrom], splitRules);

            if (splitRules.size() > 1)
            {
                result.graphs.first.addEdge(ruleFrom, ruleTo);
                result.graphs.second.addEdge(ruleTo, ruleFrom);
            }
        }
        
        bool isReliance = restrainReliance(markedRules[ruleFrom], variableCountFrom, markedRules[ruleTo], variableCountTo, strat);
        if (isReliance)
        {
            result.graphs.first.addEdge(ruleFrom, ruleTo);
            result.graphs.second.addEdge(ruleTo, ruleFrom);
        }

        if (((strat & RelianceStrategy::PairHash) > 0) && (resultCache.size() < rulePairCacheSize))
        {
            resultCache[stringHash] = isReliance;
        }

        if (globalRestraintIsTimeout)
        {
            std::cout << "Timeout-Rule-Restraint: " 
                << rules[ruleFrom].tostring() << " -> " << rules[ruleTo].tostring() << '\n'; 

            result.numberOfCalls = numCalls;
            result.timeout = true;
            return RelianceExecutionCommand::Return;
        }

        return RelianceExecutionCommand::None;
    };

    if ((strat & RelianceStrategy::CutPairs) > 0)
    {
        for (size_t ruleFrom = 0; ruleFrom < rules.size(); ++ruleFrom)
        {
            std::unordered_set<size_t> proccesedRules;

            for (const Literal &currentLiteral : rules[ruleFrom].getHeads())
            {
                PredId_t currentPredicate = currentLiteral.getPredicate().getId();
                auto toIterator = headToMap.find(currentPredicate);

                if (toIterator == headToMap.end())
                    continue;
                
                for (size_t ruleTo : toIterator->second)
                {
                    switch (relianceExecution(ruleFrom, ruleTo, proccesedRules))
                    {
                        case RelianceExecutionCommand::Return:
                            return result;
                        case RelianceExecutionCommand::Continue:
                            continue;
                    }
                }
            }
        }
    }
    else
    {
        std::unordered_set<size_t> proccesedRules;

        for (size_t ruleFrom = 0; ruleFrom < rules.size(); ++ruleFrom)
        {
            for (size_t ruleTo = 0; ruleTo < rules.size(); ++ruleTo)
            {
                switch (relianceExecution(ruleFrom, ruleTo, proccesedRules))
                {
                    case RelianceExecutionCommand::Return:
                        return result;
                    case RelianceExecutionCommand::Continue:
                        continue;
                }
            }
        }
    }

    

    result.timeMilliSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - timepointStart).count();
    result.timeout = false;
    result.numberOfCalls = numCalls;

    return result;
}
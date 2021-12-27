#include "vlog/reliances/reliances.h"

#include <vector>
#include <utility>
#include <list>
#include <unordered_map>
#include <unordered_set>

std::chrono::system_clock::time_point globalPositiveTimepointStart;
unsigned globalPositiveTimeout = 0;
unsigned globalPositiveTimeoutCheckCount = 0;
bool globalPositiveIsTimeout = false;

bool positiveExtendAssignment(const Literal &literalFrom, const Literal &literalTo,
    VariableAssignments &assignments)
{
    unsigned tupleSize = literalFrom.getTupleSize(); //Should be the same as literalTo.getTupleSize()

    for (unsigned termIndex = 0; termIndex < tupleSize; ++termIndex)
    {
        VTerm fromTerm = literalFrom.getTermAtPos(termIndex);
        VTerm toTerm = literalTo.getTermAtPos(termIndex);
    
        TermInfo fromInfo = getTermInfoUnify(fromTerm, assignments, RelianceRuleRelation::From);
        TermInfo toInfo = getTermInfoUnify(toTerm, assignments, RelianceRuleRelation::To);

        // We may not assign a universal variable of fromRule to a null
        if (fromInfo.type == TermInfo::Types::Universal && toInfo.constant < 0)
            return false;

        if (!unifyTerms(fromInfo, toInfo, assignments))
            return false;
    }

    assignments.finishGroupAssignments();

    return true;
}

bool positiveCheckNullsInToBody(const std::vector<Literal> &literals,
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

bool positiveExtend(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo,
    const VariableAssignments &assignments,
    RelianceStrategy strat);

bool positiveCheck(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo,
    const VariableAssignments &assignments,
    RelianceStrategy strat)
{
    unsigned nextInDomainIndex = 0;
    const std::vector<Literal> toBodyLiterals = ruleTo.getBody();
    std::vector<Literal> notMappedToBodyLiterals;
    notMappedToBodyLiterals.reserve(toBodyLiterals.size());
    for (unsigned bodyIndex = 0; bodyIndex < toBodyLiterals.size(); ++bodyIndex)
    {
        if (bodyIndex == mappingDomain[nextInDomainIndex])
        {
            if (nextInDomainIndex < mappingDomain.size() - 1)
            {
                ++nextInDomainIndex;
            }

            continue;
        }

        notMappedToBodyLiterals.push_back(toBodyLiterals[bodyIndex]);
    }

    if (!positiveCheckNullsInToBody(ruleFrom.getBody(), assignments, RelianceRuleRelation::From))
    {
        if ((strat & RelianceStrategy::EarlyTermination) > 0)
            return false;
        else
            return positiveExtend(mappingDomain, ruleFrom, ruleTo, assignments, strat);
    }

    if (!positiveCheckNullsInToBody(notMappedToBodyLiterals, assignments, RelianceRuleRelation::To))
    {
        return positiveExtend(mappingDomain, ruleFrom, ruleTo, assignments, strat);
    }

    std::vector<std::vector<std::unordered_map<int64_t, TermInfo>>> existentialMappings;
    std::vector<unsigned> satisfied;
    satisfied.resize(ruleFrom.getHeads().size(), 0);
    prepareExistentialMappings(ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, existentialMappings);

    //TODO: Maybe do a quick check whether or not this can fire, for example if the head contains a predicate which is not in I_a
    bool fromRuleSatisfied = relianceModels(ruleFrom.getBody(), RelianceRuleRelation::From, ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings);
    fromRuleSatisfied |= relianceModels(notMappedToBodyLiterals, RelianceRuleRelation::To, ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings);

    if (!fromRuleSatisfied && ruleFrom.isExistential())
    {
        fromRuleSatisfied = checkConsistentExistential(existentialMappings);
    }

    if (fromRuleSatisfied)
    {
        return positiveExtend(mappingDomain, ruleFrom, ruleTo, assignments, strat);
    }

    satisfied.clear();
    satisfied.resize(ruleTo.getBody().size(), 0);
    prepareExistentialMappings(ruleTo.getBody(), RelianceRuleRelation::To, assignments, existentialMappings);

    //TODO: If phi_2 contains nulls then this check can be skipped (because there are no nulls in phi_1 and no nulls in phi_{22} -> see check in the beginning for that)
    bool toBodySatisfied = relianceModels(ruleFrom.getBody(), RelianceRuleRelation::From, ruleTo.getBody(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings);
    toBodySatisfied = relianceModels(notMappedToBodyLiterals, RelianceRuleRelation::To, ruleTo.getBody(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings);

    if (!toBodySatisfied && ruleTo.isExistential())
    {
        toBodySatisfied = checkConsistentExistential(existentialMappings);
    }

    if (toBodySatisfied)
    {
        if ((strat & RelianceStrategy::EarlyTermination) > 0)
            return false;
        else
            return positiveExtend(mappingDomain, ruleFrom, ruleTo, assignments, strat);
    }

    satisfied.clear();
    satisfied.resize(ruleTo.getHeads().size(), 0);
    prepareExistentialMappings(ruleTo.getHeads(), RelianceRuleRelation::To, assignments, existentialMappings);

    bool toRuleSatisfied = relianceModels(ruleFrom.getBody(), RelianceRuleRelation::From, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings);
    toRuleSatisfied |= relianceModels(notMappedToBodyLiterals, RelianceRuleRelation::To, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings);
    toRuleSatisfied |= relianceModels(ruleFrom.getHeads(), RelianceRuleRelation::From, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings);
 
    if (!toRuleSatisfied && ruleTo.isExistential())
    {
        toRuleSatisfied = checkConsistentExistential(existentialMappings);
    }

    return !toRuleSatisfied;
}

bool positiveExtend(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo,
    const VariableAssignments &assignments,
    RelianceStrategy strat)
{
    if (globalPositiveTimeout != 0 && globalPositiveTimeoutCheckCount % 1000 == 0)
    {
        unsigned currentTimeMilliSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - globalPositiveTimepointStart).count();
        if (currentTimeMilliSeconds > globalPositiveTimeout)
        {
            globalPositiveIsTimeout = true;
            return true;
        }
    }

    ++globalPositiveTimeoutCheckCount;

    unsigned bodyToStartIndex = (mappingDomain.size() == 0) ? 0 : mappingDomain.back() + 1;

    for (unsigned bodyToIndex = bodyToStartIndex; bodyToIndex < ruleTo.getBody().size(); ++bodyToIndex)
    {
        const Literal &literalTo = ruleTo.getBody().at(bodyToIndex);
        mappingDomain.push_back(bodyToIndex);

        for (unsigned headFromIndex = 0; headFromIndex < ruleFrom.getHeads().size(); ++headFromIndex)
        {
            const Literal &literalFrom =  ruleFrom.getHeads().at(headFromIndex);

            if (literalTo.getPredicate().getId() != literalFrom.getPredicate().getId())
                continue;

            VariableAssignments extendedAssignments = assignments;
            if (!positiveExtendAssignment(literalFrom, literalTo, extendedAssignments))
                continue;

            if (positiveCheck(mappingDomain, ruleFrom, ruleTo, extendedAssignments, strat))
                return true;
        }

        mappingDomain.pop_back();
    }

    return false;
}

bool positiveReliance(const Rule &ruleFrom, unsigned variableCountFrom, const Rule &ruleTo, unsigned variableCountTo,
    RelianceStrategy strat)
{
    std::vector<unsigned> mappingDomain;
    VariableAssignments assignments(variableCountFrom, variableCountTo);

    return positiveExtend(mappingDomain, ruleFrom, ruleTo, assignments, strat);
}

RelianceComputationResult computePositiveReliances(const std::vector<Rule> &rules, RelianceStrategy strat, unsigned timeoutMilliSeconds)
{
    RelianceComputationResult result;
    result.graphs = std::pair<SimpleGraph, SimpleGraph>(SimpleGraph(rules.size()), SimpleGraph(rules.size()));

    std::vector<Rule> markedRules;
    markedRules.reserve(rules.size());

    std::vector<unsigned> variableCounts;
    variableCounts.reserve(rules.size());

    std::vector<unsigned> IDBRuleIndices;
    IDBRuleIndices.reserve(rules.size());

    for (unsigned ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex)
    {
        const Rule &currentRule = rules[ruleIndex];

        unsigned variableCount = std::max(highestLiteralsId(currentRule.getHeads()), highestLiteralsId(currentRule.getBody()));
        variableCounts.push_back(variableCount + 1);

        if (currentRule.getNIDBPredicates() > 0)
        {
            IDBRuleIndices.push_back(ruleIndex);
        }
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

    std::unordered_map<PredId_t, std::vector<size_t>> bodyToMap;
    std::unordered_map<std::string, bool> resultCache;

    for (size_t ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex)
    {
        const Rule &markedRule = markedRules[ruleIndex];

        for (const Literal &currentLiteral : markedRule.getBody())
        {
            PredId_t currentPredId = currentLiteral.getPredicate().getId();

            bodyToMap[currentPredId].push_back(ruleIndex);
        }
    }

    std::chrono::system_clock::time_point timepointStart = std::chrono::system_clock::now();
    globalPositiveTimepointStart = timepointStart;
    globalPositiveTimeout = timeoutMilliSeconds;

    uint64_t numCalls = 0;

    enum RelianceExecutionCommand {
        None,
        Continue,
        Return
    };

    auto relianceExecution = [&] (size_t ruleFrom, size_t ruleTo, std::unordered_set<size_t> &proccesedRules) {
        if ((strat & RelianceStrategy::CutPairs) > 0)
        {
            if (proccesedRules.find(ruleTo) != proccesedRules.end())
                return RelianceExecutionCommand::Continue;

            proccesedRules.insert(ruleTo);
        }

        if (globalPositiveTimeout != 0 && globalPositiveTimeoutCheckCount % 1000 == 0)
        {
            unsigned currentTimeMilliSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - globalPositiveTimepointStart).count();
            if (currentTimeMilliSeconds > globalPositiveTimeout)
            {
                globalPositiveIsTimeout = true;
                result.timeout = true;
                result.numberOfCalls = numCalls;
                return RelianceExecutionCommand::Return;
            }
        }

        ++globalPositiveTimeoutCheckCount;

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
        
        bool isReliance = positiveReliance(markedRules[ruleFrom], variableCountFrom, markedRules[ruleTo], variableCountTo, strat);
        if (isReliance)
        {
            result.graphs.first.addEdge(ruleFrom, ruleTo);
            result.graphs.second.addEdge(ruleTo, ruleFrom);
        }

        if (((strat & RelianceStrategy::PairHash) > 0) && (resultCache.size() < rulePairCacheSize))
        {
            resultCache[stringHash] = isReliance;
        }

        if (globalPositiveIsTimeout)
        {
            std::cout << "Timeout-Rule-Positive: " 
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
                auto toIterator = bodyToMap.find(currentPredicate);

                if (toIterator == bodyToMap.end())
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

unsigned DEBUGcountFakePositiveReliances(const std::vector<Rule> &rules, const SimpleGraph &positiveGraph)
{
    unsigned result = 0;

    for (unsigned ruleFrom = 0; ruleFrom < rules.size(); ++ruleFrom)
    {
        for (unsigned ruleTo = 0; ruleTo < rules.size(); ++ruleTo)
        {
            if (positiveGraph.containsEdge(ruleFrom, ruleTo))
                continue;

            bool headPredicateInBody = false;

            for (const Literal &headLiteral : rules[ruleFrom].getHeads())
            {
                for (const Literal &bodyLiteral : rules[ruleTo].getBody())
                {
                    if (headLiteral.getPredicate().getId() == bodyLiteral.getPredicate().getId())
                    {
                        headPredicateInBody = true;
                        break;
                    }
                }

                if (headPredicateInBody)
                    break;
            }

            if (headPredicateInBody)
            {
                std::cout << "Found fake reliance from " << ruleFrom << " to " << ruleTo << '\n';
                ++result;
            }
        }
    }

    return result;
}
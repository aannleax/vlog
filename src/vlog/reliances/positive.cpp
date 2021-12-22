#include "vlog/reliances/reliances.h"

#include <vector>
#include <utility>
#include <list>
#include <unordered_map>
#include <unordered_set>

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

std::pair<SimpleGraph, SimpleGraph> computePositiveReliances(const std::vector<Rule> &rules, RelianceStrategy strat)
{
    SimpleGraph result(rules.size()), resultTransposed(rules.size());
    
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

    std::unordered_map<PredId_t, std::vector<size_t>> headFromMap, bodyToMap;
    std::unordered_set<PredId_t> allPredicates;

    for (size_t ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex)
    {
        const Rule &markedRule = markedRules[ruleIndex];

        for (const Literal &currentLiteral : markedRule.getHeads())
        {
            PredId_t currentPredId = currentLiteral.getPredicate().getId();

            headFromMap[currentPredId].push_back(ruleIndex);
            allPredicates.insert(currentPredId);
        }

        for (const Literal &currentLiteral : markedRule.getBody())
        {
            PredId_t currentPredId = currentLiteral.getPredicate().getId();

            bodyToMap[currentPredId].push_back(ruleIndex);
            allPredicates.insert(currentPredId);
        }
    }

    unsigned numCalls = 0;
    std::unordered_set<uint64_t> proccesedPairs;
    for (PredId_t currentPredicate : allPredicates)
    {
        auto fromIterator = headFromMap.find(currentPredicate);
        auto toIterator = bodyToMap.find(currentPredicate);

        if (fromIterator == headFromMap.end() || toIterator == bodyToMap.end())
            continue;

        for (size_t ruleFrom : fromIterator->second)
        {
            for (size_t ruleTo : toIterator->second)
            {
        // for (size_t ruleFrom = 0; ruleFrom < rules.size(); ++ruleFrom)
        // {
        //     for (size_t ruleTo = 0; ruleTo < rules.size(); ++ruleTo)
        //     {
                uint64_t hash = ruleFrom * rules.size() + ruleTo;
                if (proccesedPairs.find(hash) != proccesedPairs.end())
                    continue;
                proccesedPairs.insert(hash);

                unsigned variableCountFrom = variableCounts[ruleFrom];
                unsigned variableCountTo = variableCounts[ruleTo];
                
                ++numCalls;
                if (positiveReliance(markedRules[ruleFrom], variableCountFrom, markedRules[ruleTo], variableCountTo, strat))
                {
                    result.addEdge(ruleFrom, ruleTo);
                    resultTransposed.addEdge(ruleTo, ruleFrom);
                }
            }
        }
    }

    std::cout << "Pos Calls: " << numCalls << '\n';

    return std::make_pair(result, resultTransposed);
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
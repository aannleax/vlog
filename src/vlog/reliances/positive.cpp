#include "vlog/reliances/reliances.h"

#include <vector>
#include <utility>
#include <list>
#include <unordered_map>
#include <unordered_set>

Rule markExistentialVariables(const Rule &rule)
{
    uint32_t ruleId = rule.getId();
    std::vector<Literal> body = rule.getBody();
    std::vector<Literal> heads;

    std::vector<Var_t> existentialVariables = rule.getExistentialVariables();
    for (const Literal &literal: rule.getHeads())
    {
        VTuple *tuple = new VTuple(literal.getTupleSize());

        for (unsigned termIndex = 0; termIndex < literal.getTupleSize(); ++termIndex)
        {
            VTerm currentTerm = literal.getTermAtPos(termIndex);;

            if (currentTerm.getId() > 0 && std::find(existentialVariables.begin(), existentialVariables.end(), currentTerm.getId()) != existentialVariables.end())
            {
                currentTerm.setId(-currentTerm.getId());
            }

            tuple->set(currentTerm, termIndex);
        }

        heads.push_back(Literal(literal.getPredicate(), *tuple, literal.isNegated()));
    }

    return Rule(ruleId, heads, body);
}

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
    const VariableAssignments &assignments)
{
    for (const Literal &literal : literals)
    {
        for (unsigned termIndex = 0; termIndex < literal.getTupleSize(); ++termIndex)
        {
            VTerm currentTerm = literal.getTermAtPos(termIndex);
        
            if ((int32_t)currentTerm.getId() > 0)
            {
                if (assignments.getConstant((int32_t)currentTerm.getId(), RelianceRuleRelation::To) < 0)
                    return false;
            }
        }
    }   

    return true;
}

bool positiveExtend(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo,
    const VariableAssignments &assignments);

bool positiveCheck(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo,
    const VariableAssignments &assignments)
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

    if (!positiveCheckNullsInToBody(notMappedToBodyLiterals, assignments))
    {
        return positiveExtend(mappingDomain, ruleFrom, ruleTo, assignments);
    }

    std::vector<std::vector<std::unordered_map<int64_t, TermInfo>>> existentialMappings;
    std::vector<unsigned> satisfied;
    satisfied.resize(ruleFrom.getHeads().size(), 0);

    //TODO: Maybe do a quick check whether or not this can fire, for example if the head contains a predicate which is not in I_a
    bool fromRuleSatisfied = 
        (relianceModels(ruleFrom.getBody(), RelianceRuleRelation::From, ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings)
        || relianceModels(notMappedToBodyLiterals, RelianceRuleRelation::To, ruleFrom.getHeads(), RelianceRuleRelation::From, assignments, satisfied, existentialMappings))
        && checkConsistentExistential(existentialMappings);

    if (fromRuleSatisfied)
    {
        return positiveExtend(mappingDomain, ruleFrom, ruleTo, assignments);
    }

    satisfied.clear();
    satisfied.resize(ruleTo.getBody().size(), 0);
    existentialMappings.clear();

    //TODO: If phi_2 contains nulls then this check can be skipped (because there are no nulls in phi_1 and no nulls in phi_{22} -> see check in the beginning for that)
    bool toBodySatisfied = 
        (relianceModels(ruleFrom.getBody(), RelianceRuleRelation::From, ruleTo.getBody(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings)
        || relianceModels(notMappedToBodyLiterals, RelianceRuleRelation::To, ruleTo.getBody(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings))
        && checkConsistentExistential(existentialMappings);

    if (toBodySatisfied)
    {
        return false;
    }

    satisfied.clear();
    satisfied.resize(ruleTo.getHeads().size(), 0);
    existentialMappings.clear();

    bool toRuleSatisfied = 
        (relianceModels(ruleFrom.getBody(), RelianceRuleRelation::From, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings)
        || relianceModels(notMappedToBodyLiterals, RelianceRuleRelation::To, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings)
        || relianceModels(ruleFrom.getHeads(), RelianceRuleRelation::From, ruleTo.getHeads(), RelianceRuleRelation::To, assignments, satisfied, existentialMappings))
        && checkConsistentExistential(existentialMappings);
    
    return !toRuleSatisfied;
}

bool positiveExtend(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo,
    const VariableAssignments &assignments)
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

            if (positiveCheck(mappingDomain, ruleFrom, ruleTo, extendedAssignments))
                return true;
        }

        mappingDomain.pop_back();
    }

    return false;
}

bool positiveReliance(const Rule &ruleFrom, unsigned variableCountFrom, const Rule &ruleTo, unsigned variableCountTo)
{
    std::vector<unsigned> mappingDomain;
    VariableAssignments assignments(variableCountFrom, variableCountTo);

    return positiveExtend(mappingDomain, ruleFrom, ruleTo, assignments);
}

std::pair<SimpleGraph, SimpleGraph> computePositiveReliances(std::vector<Rule> &rules)
{
    std::vector<Rule> markedRules;
    SimpleGraph result(rules.size()), resultTransposed(rules.size());

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
                if (ruleFrom == 7 && ruleTo == 1137)
                    int x = 0;

                uint64_t hash = ruleFrom * rules.size() + ruleTo;
                if (proccesedPairs.find(hash) != proccesedPairs.end())
                    continue;

                unsigned variableCountFrom = variableCounts[ruleFrom];
                unsigned variableCountTo = variableCounts[ruleTo];
                
                if (positiveReliance(markedRules[ruleFrom], variableCountFrom, markedRules[ruleTo], variableCountTo))
                {
                    result.addEdge(ruleFrom, ruleTo);
                    resultTransposed.addEdge(ruleTo, ruleFrom);
                }
            }
        }
    }

    return std::make_pair(result, resultTransposed);
}

unsigned DEBUGcountFakePositiveReliances(const std::vector<Rule> &rules, const SimpleGraph &positiveGraph)
{
    unsigned result = 0;

    for (unsigned ruleFrom = 0; ruleFrom < rules.size(); ++ruleFrom)
    {
        for (unsigned ruleTo = 0; ruleTo < rules.size(); ++ruleTo)
        {
            if (ruleFrom == ruleTo)
                continue;

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
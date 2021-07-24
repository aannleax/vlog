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

unsigned highestLiteralsId(const std::vector<Literal> &literalVector)
{
    unsigned result = 0;

    for (const Literal &literal: literalVector)
    {
        for (unsigned termIndex = 0; termIndex < literal.getTupleSize(); ++termIndex)
        {
            VTerm currentTerm = literal.getTermAtPos(termIndex);
        
            if (currentTerm.getId() > result)
            {
                result = currentTerm.getId();
            }
        }
    }
    
    return result;
}

void positiveChangeGroup(std::vector<int64_t> &assignments, int64_t originalGroup, int64_t newGroup)
{
    for (int64_t &currentGroup : assignments)
    {
        if (currentGroup == originalGroup)
        {
            currentGroup = newGroup;
        }
    }
}

int64_t positiveGetAssignedConstant(VTerm term, 
    const std::vector<int64_t> &assignment, const std::vector<VariableAssignments::Group> &groups)
{
    if (term.getId() == 0) // constant
        return term.getValue();
    else if ((int32_t)term.getId() < 0) // existential variable (=null)
        return (int32_t)term.getId();
    else // universal variable
      if (assignment[term.getId()] == NOT_ASSIGNED)
         return NOT_ASSIGNED;
      else
        return groups[assignment[term.getId()]].value;
}

bool positiveExtendAssignment(const Literal &literalFrom, const Literal &literalTo,
    VariableAssignments &assignments)
{
      unsigned tupleSize = literalFrom.getTupleSize(); //Should be the same as literalTo.getTupleSize()

    for (unsigned termIndex = 0; termIndex < tupleSize; ++termIndex)
    {
        VTerm fromTerm = literalFrom.getTermAtPos(termIndex);
        VTerm toTerm = literalTo.getTermAtPos(termIndex);
    
        TermInfo fromInfo = getTermInfo(fromTerm, assignments, RelianceRuleRelation::From);
        TermInfo toInfo = getTermInfo(toTerm, assignments, RelianceRuleRelation::To);

        RelianceTermCompatible compatibleInfo;
        bool compatibleResult = termsEqual(fromInfo, toInfo, assignments, &compatibleInfo);

        if (compatibleResult)
        {
            continue;
        }
        else
        {
            if (compatibleInfo.type == RelianceTermCompatible::Types::Incompatible)
            {
                return false;
            }
            else
            {
                // We may not assign a universal variable of fromRule to a null
                if (fromInfo.type == TermInfo::Types::Universal && compatibleInfo.constant < 0)
                {
                    return false;
                }

                makeCompatible(compatibleInfo, assignments.from, assignments.to, assignments.groups);
            }
        }
    }

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
                int64_t groupId = assignments.to[currentTerm.getId()];
                if (groupId == NOT_ASSIGNED)
                    continue;

                const VariableAssignments::Group &group = assignments.groups[groupId];

                if (group.value < 0)
                {
                    return false;
                }
            }
        }
    }   

    return true;
}

bool positiveModels(const std::vector<Literal> &left, const std::vector<int64_t> &leftAssignment,
    const std::vector<Literal> &right, const std::vector<int64_t> &rightAssignment,
    const std::vector<VariableAssignments::Group> &groups, 
    std::vector<unsigned> &satisfied, bool sameRule, bool treatExistentialAsConstant = false)
{
    bool isCompletelySatisfied = true;

    for (unsigned rightIndex = 0; rightIndex < right.size(); ++rightIndex)
    {   
        if (satisfied[rightIndex] == 1)
            continue;

        const Literal &rightLiteral = right[rightIndex];

        bool rightSatisfied = false;
        for (const Literal &leftLiteral : left)
        {
            if (rightLiteral.getPredicate().getId() != leftLiteral.getPredicate().getId())
                continue;

            bool leftModelsRight = true;
            unsigned tupleSize = leftLiteral.getTupleSize(); //Should be the same as rightLiteral.getTupleSize()

            for (unsigned termIndex = 0; termIndex < tupleSize; ++termIndex)
            {
                VTerm leftTerm = leftLiteral.getTermAtPos(termIndex);
                VTerm rightTerm = rightLiteral.getTermAtPos(termIndex);
           
                int64_t leftConstant = positiveGetAssignedConstant(leftTerm, leftAssignment, groups);
                int64_t rightConstant = positiveGetAssignedConstant(rightTerm, rightAssignment, groups);

                if (leftConstant != rightConstant)
                {
                    if (!treatExistentialAsConstant && rightConstant < 0)
                    {
                        continue;
                    }

                    leftModelsRight = false;
                    break;
                }
                
                if (leftConstant == NOT_ASSIGNED) //it follows that rightConstant == NOT_ASSIGNED
                {
                    int64_t leftGroup = leftAssignment[leftTerm.getId()];
                    int64_t rightGroup = rightAssignment[rightTerm.getId()];
                    
                    if (leftGroup == rightGroup)
                    {
                        if (leftGroup == NOT_ASSIGNED) //and rightGroup == NOT_ASSIGNED
                        {
                            if (sameRule && leftTerm.getId() == rightTerm.getId())
                            {
                                continue;
                            }
                            else
                            {
                                leftModelsRight = false;
                                break;
                            }
                        }
                        else
                        {
                            continue;
                        }
                    }
                    else
                    {
                        leftModelsRight = false;
                        break;
                    }
                }
                else //both are 'real' constants with the same value
                {
                    continue;
                }
            }

            if (leftModelsRight)
            {
                rightSatisfied = true;
                break;
            }
        }

        if (rightSatisfied)
        {
            satisfied[rightIndex] = 1;
        }
        else
        {
            isCompletelySatisfied = false;
        }
    }

    return isCompletelySatisfied;    
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

    std::vector<unsigned> satisfied;
    satisfied.resize(ruleFrom.getHeads().size(), 0);

    //TODO: Maybe do a quick check whether or not this can fire, for example if the head contains a predicate which is not in I_a
    bool fromRuleSatisfied = false;
    fromRuleSatisfied |= positiveModels(ruleFrom.getBody(), assignments.from, ruleFrom.getHeads(), assignments.from, assignments.groups, satisfied, true);
    fromRuleSatisfied |= positiveModels(notMappedToBodyLiterals, assignments.to, ruleFrom.getHeads(), assignments.from, assignments.groups, satisfied, false);

    if (fromRuleSatisfied)
    {
        return positiveExtend(mappingDomain, ruleFrom, ruleTo, assignments);
    }

    satisfied.clear();
    satisfied.resize(ruleTo.getBody().size(), 0);

    //TODO: If phi_2 contains nulls then this check can be skipped (because there are no nulls in phi_1 and no nulls in phi_{22} -> see check in the beginning for that)
    bool toBodySatisfied = false;
    toBodySatisfied |= positiveModels(ruleFrom.getBody(), assignments.from, ruleTo.getBody(), assignments.to, assignments.groups, satisfied, false, true);
    toBodySatisfied |= positiveModels(notMappedToBodyLiterals, assignments.to, ruleTo.getBody(), assignments.to, assignments.groups, satisfied, true, true);

    if (toBodySatisfied)
    {
        return false;
    }

    satisfied.clear();
    satisfied.resize(ruleTo.getHeads().size(), 0);

    bool toRuleSatisfied = false;
    toRuleSatisfied |= positiveModels(ruleFrom.getBody(), assignments.from, ruleTo.getHeads(), assignments.to, assignments.groups, satisfied, false);
    toRuleSatisfied |= positiveModels(notMappedToBodyLiterals, assignments.to, ruleTo.getHeads(), assignments.to, assignments.groups, satisfied, true);
    toRuleSatisfied |= positiveModels(ruleFrom.getHeads(), assignments.from, ruleTo.getHeads(), assignments.to, assignments.groups, satisfied, false);

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

std::pair<RelianceGraph, RelianceGraph> computePositiveReliances(std::vector<Rule> &rules)
{
    std::vector<Rule> markedRules;
    RelianceGraph result(rules.size()), resultTransposed(rules.size());

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
                if (ruleFrom == 11 && ruleTo == 1039)
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

unsigned DEBUGcountFakePositiveReliances(const std::vector<Rule> &rules, const RelianceGraph &positiveGraph)
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
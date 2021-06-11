#include "vlog/reliances/reliances.h"

#include <vector>
#include <utility>
#include <list>

struct VariableAssignments
{
    VariableAssignments(unsigned variableCountFrom, unsigned variableCountTo)
    {
        from.resize(variableCountFrom, NOT_ASSIGNED);
        to.resize(variableCountTo, NOT_ASSIGNED);
    }

    struct Group
    {
        Group() {}
        Group(int64_t value) {this->value = value;}

        int64_t value = NOT_ASSIGNED;
        //TODO: Maybe should include members, so that groups can be changed more easily 
    };

    std::vector<Group> groups;
    std::vector<int64_t> from, to;
};

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
    if (term.getId() == 0)
        return term.getValue();
    else if ((int32_t)term.getId() < 0)
        return (int32_t)term.getId();
    else 
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

        int64_t fromConstant = positiveGetAssignedConstant(fromTerm, assignments.from, assignments.groups);
        int64_t toConstant = positiveGetAssignedConstant(toTerm, assignments.to, assignments.groups);

        if (fromConstant != NOT_ASSIGNED && toConstant != NOT_ASSIGNED)
        {
            if (fromConstant != toConstant)
            {
                return false;
            }
            else
            {
                continue;
            }
        }
        else if ((int32_t)fromTerm.getId() > 0 && (int32_t)toTerm.getId() > 0)
        {
            int64_t &fromGroupId = assignments.from[fromTerm.getId()];
            int64_t &toGroupId = assignments.to[toTerm.getId()];
        
            if (fromGroupId== NOT_ASSIGNED && toGroupId == NOT_ASSIGNED)
            {
                int64_t newGroupId = (int64_t)assignments.groups.size();
                assignments.groups.emplace_back();

                fromGroupId = newGroupId;
                toGroupId = newGroupId;
            }
            else if (fromGroupId != NOT_ASSIGNED && toGroupId != NOT_ASSIGNED)
            {
                if (fromGroupId == toGroupId)
                {
                    continue;
                }

                int64_t fromValue = assignments.groups[fromGroupId].value;
                int64_t toValue = assignments.groups[toGroupId].value;
                
                int64_t newValue = (fromValue == NOT_ASSIGNED) ? toValue : fromValue;

                if (newValue < 0) 
                {
                    return false;
                }

                assignments.groups[fromGroupId].value = newValue;
                positiveChangeGroup(assignments.to, toGroupId, fromGroupId);
            }
            else
            {
                int64_t groupId = (fromGroupId == NOT_ASSIGNED) ? toGroupId : fromGroupId;
                int64_t &noGroupId = (fromGroupId == NOT_ASSIGNED) ? fromGroupId : toGroupId;
                VariableAssignments::Group &group = assignments.groups[groupId];
                int64_t groupConstant = group.value;

                if (fromGroupId == NOT_ASSIGNED && groupConstant < 0)
                {
                    return false;
                }

                noGroupId = groupId;
            }
        }
        else 
        {
            int64_t variableId = ((int32_t)fromTerm.getId() > 0) ? fromTerm.getId() : toTerm.getId();

            std::vector<int64_t> &assignmentVector = ((int32_t)fromTerm.getId() > 0) ? assignments.from : assignments.to;
            int64_t &groupId = assignmentVector[variableId];
            int64_t constant = ((int32_t)fromTerm.getId() > 0) ? toConstant : fromConstant;

            if (groupId == NOT_ASSIGNED)
            {
                int64_t newGroupId = (int64_t)assignments.groups.size();
                assignments.groups.emplace_back(constant);
               
                groupId = newGroupId;
            }
            else 
            {
                if (constant < 0)
                {
                    return false;
                }
                
                assignments.groups[groupId].value = constant;
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
                
                if (leftConstant == NOT_ASSIGNED) //it follows the rightConstant == NOT_ASSIGNED
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
    satisfied.resize(ruleFrom.getHeads().size());

    //TODO: Maybe do a quick check whether or not this can fire, for example if the head contains a predicate which is not in I_a
    bool fromRuleSatisfied = false;
    fromRuleSatisfied |= positiveModels(ruleFrom.getBody(), assignments.from, ruleFrom.getHeads(), assignments.from, assignments.groups, satisfied, true);
    fromRuleSatisfied |= positiveModels(notMappedToBodyLiterals, assignments.to, ruleFrom.getHeads(), assignments.from, assignments.groups, satisfied, false);

    if (fromRuleSatisfied)
    {
        return positiveExtend(mappingDomain, ruleFrom, ruleTo, assignments);
    }

    satisfied.clear();
    satisfied.resize(ruleTo.getBody().size());

    //TODO: If phi_2 contains nulls then this check can be skipped (because there are no nulls in phi_1 and no nulls in phi_{22} -> see check in the beginning for that)
    bool toBodySatisfied = false;
    toBodySatisfied |= positiveModels(ruleFrom.getBody(), assignments.from, ruleTo.getBody(), assignments.to, assignments.groups, satisfied, false, true);
    toBodySatisfied |= positiveModels(notMappedToBodyLiterals, assignments.to, ruleTo.getBody(), assignments.to, assignments.groups, satisfied, true, true);

    if (toBodySatisfied)
    {
        return false;
    }

    satisfied.clear();
    satisfied.resize(ruleTo.getHeads().size());

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

    for (const Rule &currentRule : rules)
    {
        unsigned variableCount = std::max(highestLiteralsId(currentRule.getHeads()), highestLiteralsId(currentRule.getBody()));

        variableCounts.push_back(variableCount + 1);
    }

    for (const Rule &currentRule : rules)
    {
        //TODO: Good place to handle negation as well maybe
        Rule markedRule = markExistentialVariables(currentRule);
        markedRules.push_back(markedRule);
    }

    for (unsigned ruleFrom = 0; ruleFrom < rules.size(); ++ruleFrom)
    {
        for (unsigned ruleTo = 0; ruleTo < rules.size(); ++ruleTo)
        {
            unsigned variableCountFrom = variableCounts[ruleFrom];
            unsigned variableCountTo = variableCounts[ruleTo];
            
            if (positiveReliance(markedRules[ruleFrom], variableCountFrom, markedRules[ruleTo], variableCountTo))
            {
                result.addEdge(ruleFrom, ruleTo);
                resultTransposed.addEdge(ruleTo, ruleFrom);
            }
        }
    }

    return std::make_pair(result, resultTransposed);
}

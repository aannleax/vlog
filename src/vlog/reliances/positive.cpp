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

        int64_t fromConstant = positiveGetAssignedConstant(fromTerm, assignments.from, assignments.groups);
        int64_t toConstant = positiveGetAssignedConstant(toTerm, assignments.to, assignments.groups);

        // Case 1: Both terms are either constants/nulls or variables assigned to a group which is mapped to a constant/null
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
        // Case 2: Both terms are variables where at least one of them is not in a group or in a group with not constant/null assigned
        else if ((int32_t)fromTerm.getId() > 0 && (int32_t)toTerm.getId() > 0)
        {
            int64_t &fromGroupId = assignments.from[fromTerm.getId()];
            int64_t &toGroupId = assignments.to[toTerm.getId()];
        
            // Case 2a: Both terms are variables not assigned to a group
            // We therefore need to create a new group and assign both variables to the new group
            if (fromGroupId == NOT_ASSIGNED && toGroupId == NOT_ASSIGNED)
            {
                int64_t newGroupId = (int64_t)assignments.groups.size();
                assignments.groups.emplace_back();

                fromGroupId = newGroupId;
                toGroupId = newGroupId;
            }
            // Case 2b: Both terms are variabels which have been assigned to a group
            // Note: Because case 1 is already handled there is at least one group with no assigned constant/null
            else if (fromGroupId != NOT_ASSIGNED && toGroupId != NOT_ASSIGNED)
            {
                // If both terms are in the same group then everything is fine
                if (fromGroupId == toGroupId)
                {
                    continue;
                }

                int64_t fromValue = assignments.groups[fromGroupId].value;
                int64_t toValue = assignments.groups[toGroupId].value;
                
                // If there is a group with an assigned value then newValue will equal that group's value (or NOT_ASSIGNED otherwise)
                int64_t newValue = (fromValue == NOT_ASSIGNED) ? toValue : fromValue;

                // Since nulls can only be assigned to variables in the body of toRule this must mean 
                // that we are about to assign a (universal) variable in the head of fromRule to a null which is invalid
                if (newValue < 0) 
                    return false;

                // We merge fromGroup and toGroup by assigning each member of toGroup to fromGroup
                // and setting the value of fromGroup to the value to newValue
                assignments.groups[fromGroupId].value = newValue;
                positiveChangeGroup(assignments.to, toGroupId, fromGroupId);
            }
            // Case 2c: Both terms are variables where one belongs to a group and the other one doesn't
            // Here we need to add the variable without a group to the group of the variable which is already in a group
            else
            {
                int64_t groupId = (fromGroupId == NOT_ASSIGNED) ? toGroupId : fromGroupId;
                int64_t &noGroupId = (fromGroupId == NOT_ASSIGNED) ? fromGroupId : toGroupId;
                VariableAssignments::Group &group = assignments.groups[groupId];
                int64_t groupConstant = group.value;

                // We cannot assign a universal variable from the FromRule into a group which is mapped to a null
                if (groupConstant < 0) // && fromGroupId == NOT_ASSIGNED since a variable in the head of fromRule couldn't have been assigned a null 
                    return false;

                noGroupId = groupId;
            }
        }
        // Case 3: One term is a variable with no group or a group with no value, the other term is a constant/null
        else 
        {
            int64_t variableId = ((int32_t)fromTerm.getId() > 0) ? fromTerm.getId() : toTerm.getId();

            std::vector<int64_t> &assignmentVector = ((int32_t)fromTerm.getId() > 0) ? assignments.from : assignments.to;
            int64_t &groupId = assignmentVector[variableId];
            int64_t constant = ((int32_t)fromTerm.getId() > 0) ? toConstant : fromConstant;

            // If the variable does not belong to a group then create a new group with the value of the constant/null
            if (groupId == NOT_ASSIGNED)
            {
                int64_t newGroupId = (int64_t)assignments.groups.size();
                assignments.groups.emplace_back(constant);
               
                groupId = newGroupId;
            }
            // If the variable belongs to group then set the group value to the constant
            else 
            {
                // A group always consits of at least one universal variable from fromRule which cannot be assigned a null
                if (constant < 0)
                    return false;
                
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

    for (unsigned ruleFrom = 0; ruleFrom < rules.size(); ++ruleFrom)
    {
        for (unsigned ruleTo : IDBRuleIndices)
        {
            if (ruleFrom == ruleTo)
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
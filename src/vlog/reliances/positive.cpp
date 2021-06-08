#include "vlog/reliances/reliances.h"

#include <vector>
#include <utility>

struct PositiveVariableAssignments
{
    std::vector<Assignment> from, to;
};

int positiveGroupId = 0;

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

void positiveChangeGroup(std::vector<Assignment> &assignments, int64_t originalGroup, int64_t newGroup, int64_t newValue = NOT_ASSIGNED)
{
    if (originalGroup == NOT_ASSIGNED)
        return;

    for (Assignment &currentAssignment : assignments)
    {
        if (currentAssignment.group == originalGroup)
        {
            currentAssignment.group = newGroup;
            currentAssignment.value = newValue;
        }
    }
}

void positiveAssignGroup(std::vector<Assignment> &assignments, int64_t group, int64_t newValue)
{
    if (group == NOT_ASSIGNED)
        return;

    for (Assignment &currentAssignment : assignments)
    {
        if (currentAssignment.group == group)
        {
            currentAssignment.value = newValue;
        }
    }
}

int64_t positiveGetAssignedConstant(VTerm term, const std::vector<Assignment> &assignment)
{
    if (term.getId() == 0)
        return term.getValue();
    else if ((uint32_t)term.getId() < 0)
        return term.getId();
    else 
        return assignment[term.getId()].value;
}

bool positiveExtendAssignment(const Literal &literalFrom, const Literal &literalTo,
    PositiveVariableAssignments &assignments)
{
    unsigned tupleSize = literalFrom.getTupleSize(); //Should be the same as literalTo.getTupleSize()

    for (unsigned termIndex = 0; termIndex < tupleSize; ++termIndex)
    {
        VTerm fromTerm = literalFrom.getTermAtPos(termIndex);
        VTerm toTerm = literalTo.getTermAtPos(termIndex);

        int64_t fromConstant = positiveGetAssignedConstant(fromTerm, assignments.from);
        int64_t toConstant = positiveGetAssignedConstant(toTerm, assignments.to);

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
            Assignment &fromAssignment = assignments.from[fromTerm.getId()];
            Assignment &toAssignment = assignments.to[toTerm.getId()];
        
            if (fromAssignment.group == NOT_ASSIGNED && toAssignment.group == NOT_ASSIGNED)
            {
                int64_t newGroup = positiveGroupId++;
                    
                fromAssignment.group = newGroup;
                toAssignment.group = newGroup;
            }
            else if (fromAssignment.group != NOT_ASSIGNED && toAssignment.group != NOT_ASSIGNED)
            {
                if (fromAssignment.group == toAssignment.group)
                {
                    continue;
                }

                int64_t newGroup = positiveGroupId++;
                int64_t newValue = (fromAssignment.value == NOT_ASSIGNED) ? toAssignment.value : fromAssignment.value;

                positiveChangeGroup(assignments.from, fromAssignment.group, newGroup, newValue);
                positiveChangeGroup(assignments.to, toAssignment.group, newGroup, newValue);
            }
            else
            {
                Assignment &noGroupAssignment = (fromAssignment.group == NOT_ASSIGNED) ? fromAssignment : toAssignment;
                Assignment &groupAssignment = (fromAssignment.group == NOT_ASSIGNED) ? toAssignment : fromAssignment;
            
                noGroupAssignment.group = groupAssignment.group;
                noGroupAssignment.value = groupAssignment.value;
            }
        }
        else 
        {
            std::vector<Assignment> &assignmentVector = ((int32_t)fromTerm.getId() > 0) ? assignments.from : assignments.to;
            Assignment &variableAssignment = ((int32_t)fromTerm.getId() > 0) ? assignmentVector[fromTerm.getId()] : assignmentVector[toTerm.getId()];
            int64_t constant = ((int32_t)fromTerm.getId() > 0) ? toConstant : fromConstant;

            if (variableAssignment.group == NOT_ASSIGNED)
            {
                int64_t newGroup = positiveGroupId++;

                variableAssignment.group = newGroup;
                variableAssignment.value = constant;
            }
            else 
            {
                positiveAssignGroup(assignmentVector, variableAssignment.group, constant);
            }
        }   

        if ((int32_t)fromTerm.getId() > 0)
        {
            Assignment &fromAssignment = assignments.from[fromTerm.getId()];
            if (fromAssignment.value < 0)
            {
                return false;
            }
        }
    }

    return true;   
}

bool positiveCheckNullsInToBody(std::vector<unsigned> &mappingDomain,
    const Rule &ruleTo,
    const PositiveVariableAssignments &assignments)
{
    unsigned nextInDomainIndex = 0;
    const std::vector<Literal> &toBodyLiterals = ruleTo.getBody();
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

        const Literal &literal = toBodyLiterals[bodyIndex];

        for (unsigned termIndex = 0; termIndex < literal.getTupleSize(); ++termIndex)
        {
            VTerm currentTerm = literal.getTermAtPos(termIndex);
        
            if ((int32_t)currentTerm.getId() > 0)
            {
                const Assignment &assignment = assignments.to[currentTerm.getId()];

                if (assignment.value < 0)
                {
                    return false;
                }
            }
        }
    }   
}

bool positiveModels(const std::vector<Literal> &left, const std::vector<Assignment> &leftAssignment,
    const std::vector<Literal> &right, const std::vector<Assignment> &rightAssignment,
    std::vector<unsigned> &satisfied, bool sameRule)
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
           
                int64_t leftConstant = positiveGetAssignedConstant(leftTerm, leftAssignment);
                int64_t rightConstant = positiveGetAssignedConstant(rightTerm, rightAssignment);

                if (leftConstant != rightConstant)
                {
                    leftModelsRight = false;
                    break;
                }
                
                if (leftConstant == NOT_ASSIGNED) //it follows the rightConstant == NOT_ASSIGNED
                {
                    int64_t leftGroup = leftAssignment[leftTerm.getId()].group;
                    int64_t rightGroup = rightAssignment[rightTerm.getId()].group;
                    
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
    const PositiveVariableAssignments &assignments);

bool positiveCheck(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo,
    const PositiveVariableAssignments &assignments)
{
    if (!positiveCheckNullsInToBody(mappingDomain, ruleTo, assignments))
    {
        return positiveExtend(mappingDomain, ruleFrom, ruleTo, assignments);
    }

    const std::vector<Literal> toBodyLiterals = ruleTo.getBody();
    std::vector<Literal> mappedToBodyLiterals;
    for (unsigned domainElement : mappingDomain)
    {
        mappedToBodyLiterals.push_back(toBodyLiterals[domainElement]);
    }

    std::vector<unsigned> satisfied;
    satisfied.resize(ruleFrom.getHeads().size());

    bool fromRuleSatisfied = false;
    fromRuleSatisfied |= positiveModels(ruleFrom.getBody(), assignments.from, ruleFrom.getHeads(), assignments.from, satisfied, true);
    fromRuleSatisfied |= positiveModels(mappedToBodyLiterals, assignments.to, ruleFrom.getHeads(), assignments.from, satisfied, false);

    if (fromRuleSatisfied)
    {
        return positiveExtend(mappingDomain, ruleFrom, ruleTo, assignments);
    }

    satisfied.clear();
    satisfied.resize(ruleTo.getHeads().size());

    bool toRuleSatisfied = false;
    toRuleSatisfied |= positiveModels(ruleFrom.getBody(), assignments.from, ruleTo.getHeads(), assignments.to, satisfied, false);
    toRuleSatisfied |= positiveModels(mappedToBodyLiterals, assignments.to, ruleTo.getHeads(), assignments.to, satisfied, true);
    toRuleSatisfied |= positiveModels(ruleFrom.getHeads(), assignments.from, ruleTo.getHeads(), assignments.to, satisfied, false);

    return !toRuleSatisfied;
}

bool positiveExtend(std::vector<unsigned> &mappingDomain, 
    const Rule &ruleFrom, const Rule &ruleTo,
    const PositiveVariableAssignments &assignments)
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

            PositiveVariableAssignments extendedAssignments = assignments;
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
    std::vector<Assignment> fromAssignment, toAssignment;

    fromAssignment.resize(variableCountFrom);
    toAssignment.resize(variableCountTo);

    PositiveVariableAssignments assignments;
    assignments.from = fromAssignment;
    assignments.to = toAssignment;

    positiveGroupId = 0;

    return positiveExtend(mappingDomain, ruleFrom, ruleTo, assignments);
}

std::pair<RelianceGraph, RelianceGraph> computePositiveReliances(Program *program)
{
    std::vector<Rule> rules = program->getAllRules();
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

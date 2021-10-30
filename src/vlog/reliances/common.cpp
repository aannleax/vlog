#include "vlog/reliances/reliances.h"

#include <vector>
#include <utility>
#include <list>
#include <unordered_map>
#include <unordered_set>

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

TermInfo getTermInfoUnify(VTerm term, const VariableAssignments &assignments, RelianceRuleRelation relation)
{
    TermInfo result;
    result.relation = relation;
    result.termId = (int32_t)term.getId();

    if (term.getId() == 0)
    {
        result.constant = (int64_t)term.getValue();
        result.type = TermInfo::Types::Constant;
    }
    else
    {
        if ((int32_t)term.getId() < 0 && relation == RelianceRuleRelation::From)
        {
            result.constant = (int32_t)term.getId();
            result.type = TermInfo::Types::Constant;
        }
        else
        {
            result.type = ((int32_t)term.getId() > 0) ? TermInfo::Types::Universal : TermInfo::Types::Existential;
            result.groupId = assignments.getGroupId(term.getId(), relation);
            result.constant = assignments.getConstant(term.getId(), relation);
        }
    }

    return result;
}

TermInfo getTermInfoModels(VTerm term, const VariableAssignments &assignments, RelianceRuleRelation relation,
    bool alwaysDefaultAssignExistentials)
{
    TermInfo result;
    result.relation = relation;
    result.termId = (int32_t)term.getId();

    if (term.getId() == 0)
    {
        result.type = TermInfo::Types::Constant;
        result.constant = (int64_t)term.getValue();
    }
    else if ((int32_t)term.getId() > 0)
    {
        result.type = TermInfo::Types::Universal;

        result.groupId = assignments.getGroupId(term.getId(), relation);
        result.constant = assignments.getConstant(term.getId(), relation);
    }
    else
    {
        result.type = TermInfo::Types::Existential;

        if (alwaysDefaultAssignExistentials)
        {
            result.constant = (int32_t)term.getId();
        }
        else
        {
            result.groupId = assignments.getGroupId(term.getId(), relation);
            result.constant = assignments.getConstant(term.getId(), relation);
        
            if (result.groupId == NOT_ASSIGNED && result.constant == NOT_ASSIGNED)
            {
                result.constant = (int32_t)term.getId();
            }
        }
    }

    return result;
}

bool termsEqual(const TermInfo &termLeft, const TermInfo &termRight)
{
    if (termLeft.type == TermInfo::Types::Constant || termRight.type == TermInfo::Types::Constant)
        return termLeft.constant == termRight.constant;

    if (termLeft.constant != NOT_ASSIGNED && termLeft.constant == termRight.constant)
        return true;
    
    if (termLeft.groupId != NOT_ASSIGNED && termLeft.groupId == termRight.groupId)
        return true;

    if (termLeft.groupId == NOT_ASSIGNED && termRight.groupId == NOT_ASSIGNED
        && termLeft.termId == termRight.termID && termLeft.relation == termRight.relation)
        return true;

    return false;

    // if (termLeft.constant != NOT_ASSIGNED && termRight.constant != NOT_ASSIGNED)
    // {
    //     if (termLeft.constant != termRight.constant)
    //         return false;
    //     else 
    //         return termLeft.constant >= 0 || termLeft.relation == termRight.relation;
    // }
    // else
    // {
    //     if (compatible != nullptr)
    //         compatible->constant = (termLeft.constant != NOT_ASSIGNED) ? termLeft.constant : termRight.constant;
    // }

    // if (termLeft.groupId != NOT_ASSIGNED && termRight.groupId != NOT_ASSIGNED)
    // {
    //     if (termLeft.groupId == termRight.groupId)
    //     {
    //         return true;   
    //     }
    //     else
    //     {
    //         if (compatible != nullptr)
    //         {
    //             compatible->type = RelianceTermCompatible::Types::MergeGroups;
    //             compatible->mergeGroups.groupLeftId = termLeft.groupId;
    //             compatible->mergeGroups.groupRightId = termRight.groupId;
    //         }
            
    //         return false;
    //     }
    // }

    // if (termLeft.termId == termRight.termId && termLeft.relation == termRight.relation)
    //     return true;

    // if (compatible == nullptr)
    //     return false;

    // if (termLeft.type == TermInfo::Types::Constant || termRight.type == TermInfo::Types::Constant)
    // {
    //     int64_t groupId = (termLeft.type == TermInfo::Types::Constant) ? termRight.groupId : termLeft.groupId;

    //     if (groupId != NOT_ASSIGNED)
    //     {
    //         compatible->type = RelianceTermCompatible::Types::GroupSetConstant;
    //         compatible->setConstant.groupId = groupId;
    //     }
    //     else
    //     {
    //         compatible->type = RelianceTermCompatible::Types::NewGroup;
    //         compatible->newGroup.variableLeftId = (termLeft.type == TermInfo::Types::Constant) ? NOT_ASSIGNED : std::abs(termLeft.termId);
    //         compatible->newGroup.variableRightId = (termRight.type == TermInfo::Types::Constant) ? NOT_ASSIGNED : std::abs(termRight.termId);
    //     }
    // }
    // else
    // {
    //     if (termLeft.groupId != NOT_ASSIGNED && termRight.groupId == NOT_ASSIGNED)
    //     {
    //         compatible->type = RelianceTermCompatible::Types::AddVariableRightToGroup;
    //         compatible->addToGroup.variableId = std::abs(termRight.termId);
    //         compatible->addToGroup.groupId = termLeft.groupId;
    //     }
    //     else if (termLeft.groupId == NOT_ASSIGNED && termRight.groupId != NOT_ASSIGNED)
    //     {
    //         compatible->type = RelianceTermCompatible::Types::AddVariableLeftToGroup;
    //         compatible->addToGroup.variableId = std::abs(termLeft.termId);
    //         compatible->addToGroup.groupId = termRight.groupId;
    //     }
    //     else
    //     {
    //         compatible->type = RelianceTermCompatible::Types::NewGroup;
    //         compatible->newGroup.variableLeftId = std::abs(termLeft.termId);
    //         compatible->newGroup.variableRightId = std::abs(termRight.termId);
    //     }
    // }

    // return false;
}

// void relianceMergeGroup(std::vector<int64_t> &assignments, int64_t originalGroup, int64_t newGroup)
// {
//     for (int64_t &currentGroup : assignments)
//     {
//         if (currentGroup == originalGroup)
//         {
//             currentGroup = newGroup;
//         }
//     }
// }

// void makeCompatible(const RelianceTermCompatible &compatibleInfo, 
//     std::vector<int64_t> &assignmentLeft, std::vector<int64_t> &assignmentRight,
//     std::vector<VariableAssignments::Group> &groups)
// {
//     switch (compatibleInfo.type)
//     {
//         case RelianceTermCompatible::Types::AddVariableLeftToGroup:
//         {
//             assignmentLeft[compatibleInfo.addToGroup.variableId] = compatibleInfo.addToGroup.groupId;
//         } break;

//         case RelianceTermCompatible::Types::AddVariableRightToGroup:
//         {
//             assignmentRight[compatibleInfo.addToGroup.variableId] = compatibleInfo.addToGroup.groupId;
//         } break;

//         case RelianceTermCompatible::Types::GroupSetConstant:
//         {
//             groups[compatibleInfo.setConstant.groupId] = compatibleInfo.constant;
//         } break;

//         case RelianceTermCompatible::Types::NewGroup:
//         {
//             int64_t newGroupId = (int64_t)groups.size();
//             groups.emplace_back(compatibleInfo.constant);
            
//             if (compatibleInfo.newGroup.variableLeftId != NOT_ASSIGNED)
//                 assignmentLeft[compatibleInfo.newGroup.variableLeftId] = newGroupId;
//             if (compatibleInfo.newGroup.variableRightId != NOT_ASSIGNED)
//                 assignmentRight[compatibleInfo.newGroup.variableRightId] = newGroupId;
//         } break;

//         case RelianceTermCompatible::Types::MergeGroups:
//         {
//             groups[compatibleInfo.mergeGroups.groupRightId].value = compatibleInfo.constant;
//             relianceMergeGroup(assignmentLeft, compatibleInfo.mergeGroups.groupLeftId, compatibleInfo.mergeGroups.groupRightId);
//         } break;
//     }
// }

bool unifyTerms(const TermInfo &fromInfo, const TermInfo &toInfo, VariableAssignments &assignments)
{
    if (fromInfo.constant != NOT_ASSIGNED && toInfo.constant != NOT_ASSIGNED && fromInfo.constant != toInfo.constant)
        return false;

    if (fromInfo.type == TermInfo::Types::Constant && toInfo.type == TermInfo::Types::Constant)
        return true;

    if (fromInfo.type != TermInfo::Types::Constant && toInfo.type == TermInfo::Types::Constant)
    {
        assignments.assignConstants(fromInfo.termId, fromInfo.relation, toInfo.constant);
    }
    else if (fromInfo.type == TermInfo::Types::Constant && toInfo.type != TermInfo::Types::Constant)
    {
        assignments.assignConstants(toInfo.termId, toInfo.relation, fromInfo.constant);
    }
    else 
    {
        assignments.connectVariables(fromInfo.termId, toInfo.termId);
    }

    return true;
}

void prepareExistentialMappings(const std::vector<Literal> &right, RelianceRuleRelation rightRelation,
    const VariableAssignments &assignments,
    std::vector<std::vector<std::unordered_map<int64_t, TermInfo>>> &existentialMappings)
{
    existentialMappings.clear();

    for (unsigned rightIndex = 0; rightIndex < right.size(); ++rightIndex)
    {      
        const Literal &rightLiteral = right[rightIndex];
        unsigned tupleSize = rightLiteral.getTupleSize(); 

        for (unsigned termIndex = 0; termIndex < tupleSize; ++termIndex)
        {
            VTerm rightTerm = rightLiteral.getTermAtPos(termIndex);
            TermInfo rightInfo = getTermInfoModels(rightTerm, assignments, rightRelation, false);
     
            if (rightInfo.type == TermInfo::Types::Existential)
            {
                existentialMappings.emplace_back();
                break;
            }
        }
    }
}

bool relianceModels(const std::vector<Literal> &left, RelianceRuleRelation leftRelation,
    const std::vector<Literal> &right, RelianceRuleRelation rightRelation,
    const VariableAssignments &assignments,
    std::vector<unsigned> &satisfied, std::vector<std::vector<std::unordered_map<int64_t, TermInfo>>> &existentialMappings,
    bool alwaysDefaultAssignExistentials,
    bool treatExistentialAsVariables
)
{
    bool isCompletelySatisfied = true; //refers to non-existential atoms
    
    size_t existentialMappingIndex = 0;

    for (unsigned rightIndex = 0; rightIndex < right.size(); ++rightIndex)
    {   
        if (satisfied[rightIndex] == 1)
            continue;

        const Literal &rightLiteral = right[rightIndex];
        
        bool rightSatisfied = false;
        bool rightExistential = false;
        for (const Literal &leftLiteral : left)
        {
            if (rightLiteral.getPredicate().getId() != leftLiteral.getPredicate().getId())
                continue;

            std::unordered_map<int64_t, TermInfo> *currentMapping = nullptr;

            bool leftModelsRight = true;
            unsigned tupleSize = leftLiteral.getTupleSize(); //Should be the same as rightLiteral.getTupleSize()

            for (unsigned termIndex = 0; termIndex < tupleSize; ++termIndex)
            {
                VTerm leftTerm = leftLiteral.getTermAtPos(termIndex);
                VTerm rightTerm = rightLiteral.getTermAtPos(termIndex);

                TermInfo leftInfo = getTermInfoModels(leftTerm, assignments, leftRelation, alwaysDefaultAssignExistentials);
                TermInfo rightInfo = getTermInfoModels(rightTerm, assignments, rightRelation, false); //TODO: Rethink order of for loops in order to save this computation

                if (treatExistentialAsVariables && rightInfo.type == TermInfo::Types::Existential)
                {
                    rightExistential = true;

                    if (currentMapping == nullptr)
                    {   
                        auto &currentMappingVector = existentialMappings[existentialMappingIndex];
                        currentMappingVector.emplace_back();
                        currentMapping = &currentMappingVector.back();
                    }
                 
                    auto mapIterator = currentMapping->find(rightInfo.termId);
                    if (mapIterator == currentMapping->end())
                    {
                        (*currentMapping)[rightInfo.termId] = leftInfo;
                    }
                    else
                    {
                        if (termsEqual(mapIterator->second, leftInfo))
                        {
                            continue;
                        }
                        else
                        {
                            leftModelsRight = false;
                            break;
                        }
                    }
                }
                else
                {
                    if (termsEqual(leftInfo, rightInfo))
                    {
                        continue;
                    }
                    else
                    {
                        leftModelsRight = false;
                        break;
                    }
                }
            }

            if (leftModelsRight && !rightExistential)
            {
                rightSatisfied = true;
                break;
            }

            if (rightExistential && !leftModelsRight && currentMapping != nullptr)
            {
                existentialMappings[existentialMappingIndex].pop_back();
            }
        }

        if (rightExistential)
            ++existentialMappingIndex;
        else
        {
            if (rightSatisfied)
                satisfied[rightIndex] = 1;
            else
                isCompletelySatisfied = false;
        }
      
    }

    return isCompletelySatisfied;
}

bool isMappingConsistent(std::unordered_map<int64_t, TermInfo> &mapping, 
    const std::unordered_map<int64_t, TermInfo> &compare)
{
    for (auto compareIterator : compare)
    {
        auto mappingIterator = mapping.find(compareIterator.first);
        if (mappingIterator == mapping.end())
        {
            mapping[compareIterator.first] = compareIterator.second;
        }
        else
        {
            if (!termsEqual(mappingIterator->second, compareIterator.second))
                return false;
        }
    }

    return true;
}

bool checkConsistentExistentialDeep(const std::unordered_map<int64_t, TermInfo> &currentMapping, 
    const std::vector<std::vector<std::unordered_map<int64_t, TermInfo>>> &mappings,
    size_t nextIndex)
{
    if (nextIndex >= mappings.size())
        return true;

    for (const std::unordered_map<int64_t, TermInfo> &nextMap : mappings[nextIndex])
    {
        std::unordered_map<int64_t, TermInfo> updatedMap = currentMapping;
        if (isMappingConsistent(updatedMap, nextMap))
        {
            if (checkConsistentExistentialDeep(updatedMap, mappings, nextIndex + 1))
                return true;
        }
    }

    return false;
}

bool checkConsistentExistential(const std::vector<std::vector<std::unordered_map<int64_t, TermInfo>>> &mappings)
{
    if (mappings.size() <= 1)
        return false;

    for (auto &possibleMappings : mappings)
    {
        if (possibleMappings.size() == 0)
            return false;
    }

    for (const std::unordered_map<int64_t, TermInfo> &startMap : mappings[0])
    {
        if (checkConsistentExistentialDeep(startMap, mappings, 1))
            return true;
    }

    return false;
}
#include "vlog/reliances/reliances.h"

#include <vector>
#include <utility>
#include <list>
#include <unordered_map>
#include <unordered_set>

TermInfo getTermInfo(VTerm term, const VariableAssignments &assignments, RelianceRuleRelation relation)
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
            
            if (result.groupId != NOT_ASSIGNED)
                result.constant = assignments.groups[result.groupId].value;
        }
    }

    return result;
}

bool termsEqual(const TermInfo &termLeft, const TermInfo &termRight, const VariableAssignments &assignments,
    RelianceTermCompatible *compatible)
{
    if (termLeft.constant != NOT_ASSIGNED && termRight.constant != NOT_ASSIGNED)
    {
        if (termLeft.constant != termRight.constant)
            return false;
        else 
            return termLeft.constant >= 0 || termLeft.relation == termRight.relation;
    }
    else
    {
        if (compatible != nullptr)
            compatible->constant = (termLeft.constant != NOT_ASSIGNED) ? termLeft.constant : termRight.constant;
    }

    if (termLeft.groupId != NOT_ASSIGNED && termRight.groupId != NOT_ASSIGNED)
    {
        if (termLeft.groupId == termRight.groupId)
        {
            return true;   
        }
        else
        {
            if (compatible != nullptr)
            {
                compatible->type = RelianceTermCompatible::Types::MergeGroups;
                compatible->mergeGroups.groupLeftId = termLeft.groupId;
                compatible->mergeGroups.groupRightId = termRight.groupId;
            }
            
            return false;
        }
    }

    if (termLeft.termId == termRight.termId && termLeft.relation == termRight.relation)
        return true;

    if (compatible == nullptr)
        return false;

    if (termLeft.type == TermInfo::Types::Constant || termRight.type == TermInfo::Types::Constant)
    {
        int64_t groupId = (termLeft.type == TermInfo::Types::Constant) ? termRight.groupId : termLeft.groupId;

        if (groupId != NOT_ASSIGNED)
        {
            compatible->type = RelianceTermCompatible::Types::GroupSetConstant;
            compatible->setConstant.groupId = groupId;
        }
        else
        {
            compatible->type = RelianceTermCompatible::Types::NewGroup;
            compatible->newGroup.variableLeftId = (termLeft.type == TermInfo::Types::Constant) ? NOT_ASSIGNED : std::abs(termLeft.termId);
            compatible->newGroup.variableRightId = (termRight.type == TermInfo::Types::Constant) ? NOT_ASSIGNED : std::abs(termRight.termId);
        }
    }
    else
    {
        if (termLeft.groupId != NOT_ASSIGNED && termRight.groupId == NOT_ASSIGNED)
        {
            compatible->type = RelianceTermCompatible::Types::AddVariableRightToGroup;
            compatible->addToGroup.variableId = std::abs(termRight.termId);
            compatible->addToGroup.groupId = termLeft.groupId;
        }
        else if (termLeft.groupId == NOT_ASSIGNED && termRight.groupId != NOT_ASSIGNED)
        {
            compatible->type = RelianceTermCompatible::Types::AddVariableLeftToGroup;
            compatible->addToGroup.variableId = std::abs(termLeft.termId);
            compatible->addToGroup.groupId = termRight.groupId;
        }
        else
        {
            compatible->type = RelianceTermCompatible::Types::NewGroup;
            compatible->newGroup.variableLeftId = std::abs(termLeft.termId);
            compatible->newGroup.variableRightId = std::abs(termRight.termId);
        }
    }

    return false;
}

void relianceMergeGroup(std::vector<int64_t> &assignments, int64_t originalGroup, int64_t newGroup)
{
    for (int64_t &currentGroup : assignments)
    {
        if (currentGroup == originalGroup)
        {
            currentGroup = newGroup;
        }
    }
}

void makeCompatible(const RelianceTermCompatible &compatibleInfo, 
    std::vector<int64_t> &assignmentLeft, std::vector<int64_t> &assignmentRight,
    std::vector<VariableAssignments::Group> &groups)
{
    switch (compatibleInfo.type)
    {
        case RelianceTermCompatible::Types::AddVariableLeftToGroup:
        {
            assignmentLeft[compatibleInfo.addToGroup.variableId] = compatibleInfo.addToGroup.groupId;
        } break;

        case RelianceTermCompatible::Types::AddVariableRightToGroup:
        {
            assignmentRight[compatibleInfo.addToGroup.variableId] = compatibleInfo.addToGroup.groupId;
        } break;

        case RelianceTermCompatible::Types::GroupSetConstant:
        {
            groups[compatibleInfo.setConstant.groupId] = compatibleInfo.constant;
        } break;

        case RelianceTermCompatible::Types::NewGroup:
        {
            int64_t newGroupId = (int64_t)groups.size();
            groups.emplace_back(compatibleInfo.constant);
            
            if (compatibleInfo.newGroup.variableLeftId != NOT_ASSIGNED)
                assignmentLeft[compatibleInfo.newGroup.variableLeftId] = newGroupId;
            if (compatibleInfo.newGroup.variableRightId != NOT_ASSIGNED)
                assignmentRight[compatibleInfo.newGroup.variableRightId] = newGroupId;
        } break;

        case RelianceTermCompatible::Types::MergeGroups:
        {
            groups[compatibleInfo.mergeGroups.groupRightId].value = compatibleInfo.constant;
            relianceMergeGroup(assignmentLeft, compatibleInfo.mergeGroups.groupLeftId, compatibleInfo.mergeGroups.groupRightId);
        } break;
    }
}
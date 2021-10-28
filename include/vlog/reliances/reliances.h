#ifndef _RELIENCES_H
#define _RELIENCES_H

#include "vlog/concepts.h"

#include <vector>
#include <limits>
#include <fstream>
#include <string>

#define NOT_ASSIGNED std::numeric_limits<int64_t>::max()

struct RelianceGraph
{
    std::vector<std::vector<unsigned>> edges;

    RelianceGraph(unsigned nodeCount)
    {
        edges.resize(nodeCount);
    } 

    void addEdge(unsigned from, unsigned to)
    {
        edges[from].push_back(to);
    }

    void saveCSV(const std::string &filename) const
    {
        std::ofstream stream(filename);

        for (unsigned from = 0; from < edges.size(); ++from)
        {
            for (unsigned to : edges[from])
            {
                stream << from << "," << to << std::endl;
            }
        }
    }

    bool containsEdge(unsigned from, unsigned to) const
    {
        if (edges[from].size() > 0)
        {
            auto iter = std::find(edges[from].begin(), edges[from].end(), to);
            return iter != edges[from].end();
        }

        return false;
    }
};

enum class RelianceRuleRelation
{
    From, To
};

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

    int64_t getGroupId(int32_t variableId, RelianceRuleRelation relation) const
    {
        if (relation == RelianceRuleRelation::From)
            return from[std::abs(variableId)];
        else 
            return to[std::abs(variableId)];
    }

    Group &getGroup(int64_t groupId) { return groups[groupId]; }
    const Group &getGroup(int64_t groupId) const { return groups[groupId]; }

    std::vector<Group> groups;
    std::vector<int64_t> from, to;
};

struct RelianceTermCompatible
{
    enum Types
    {
        Incompatible,
        AddVariableLeftToGroup,
        AddVariableRightToGroup,
        MergeGroups,
        NewGroup,
        GroupSetConstant
    } type = Types::Incompatible;

    union
    {
        struct Add
        {
            int64_t variableId;
            int64_t groupId;   
        } addToGroup;

        struct Merge
        {
            int64_t groupLeftId;
            int64_t groupRightId;
        } mergeGroups;

        struct New
        {
            int64_t variableLeftId;
            int64_t variableRightId;
        } newGroup;

        struct Set
        {
            int64_t groupId;
            int64_t placeHolder;
        } setConstant;
    };

     int64_t constant;
};

struct TermInfo
{
    enum Types
    {
        Constant, Universal, Existential
    } type;

    int64_t constant = NOT_ASSIGNED;
    int64_t groupId = NOT_ASSIGNED;
    int64_t termId;
    RelianceRuleRelation relation;
};

// Common
TermInfo getTermInfo(VTerm term, const VariableAssignments &assignments, RelianceRuleRelation relation);
bool termsEqual(const TermInfo &termLeft, const TermInfo &termRight, RelianceTermCompatible *compatible = nullptr);
void makeCompatible(const RelianceTermCompatible &compatibleInfo, std::vector<int64_t> &assignmentLeft, std::vector<int64_t> &assignmentRight, std::vector<VariableAssignments::Group> &groups);
unsigned highestLiteralsId(const std::vector<Literal> &literalVector);
bool checkConsistentExistential(const std::vector<std::vector<std::unordered_map<int64_t, TermInfo>>> &mappings);
bool relianceModels(const std::vector<Literal> &left, RelianceRuleRelation leftRelation, const std::vector<Literal> &right, RelianceRuleRelation rightRelation, const VariableAssignments &assignments, std::vector<unsigned> &satisfied, std::vector<std::vector<std::unordered_map<int64_t, TermInfo>>> &existentialMappings);

// For outside
std::pair<RelianceGraph, RelianceGraph> computePositiveReliances(std::vector<Rule> &rules);
std::pair<RelianceGraph, RelianceGraph> computeRestrainReliances(std::vector<Rule> &rules);
unsigned DEBUGcountFakePositiveReliances(const std::vector<Rule> &rules, const RelianceGraph &positiveGraph);

#endif
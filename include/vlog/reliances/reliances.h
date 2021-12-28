#ifndef _RELIENCES_H
#define _RELIENCES_H

#include "vlog/concepts.h"

#include <vector>
#include <limits>
#include <fstream>
#include <string>
#include <algorithm>
#include <numeric>
#include <stack>

#define NOT_ASSIGNED std::numeric_limits<int64_t>::max()
#define ASSIGNED (NOT_ASSIGNED - 1)

enum RelianceStrategy : int32_t
{
    Naive = 0,
    EarlyTermination = 1,
    CutPairs = 2,
    PairHash = 4,
    BetterIterate = 8,
    Full = 15
};

struct SimpleGraph
{
    std::vector<std::vector<size_t>> edges;
    std::vector<unsigned> nodes;
    unsigned numberOfInitialNodes;

    SimpleGraph() {};

    SimpleGraph(size_t nodeCount)
    {
        numberOfInitialNodes = nodeCount;

        edges.resize(nodeCount);
        nodes.resize(nodeCount);

        std::iota(nodes.begin(), nodes.end(), 0);
    } 

    void addEdge(size_t from, size_t to)
    {
        edges[from].push_back(to);
    }

    void saveCSV(const std::string &filename) const
    {
        std::ofstream stream(filename);

        for (size_t from = 0; from < edges.size(); ++from)
        {
            for (size_t to : edges[from])
            {
                stream << from << "," << to << std::endl;
            }
        }
    }

    bool containsEdge(size_t from, size_t to) const
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

struct RelianceComputationResult
{
    std::pair<SimpleGraph, SimpleGraph> graphs;
    uint64_t numberOfCalls;
    bool timeout;
    unsigned timeMilliSeconds = 0;
};

enum class RelianceCheckResult
{
    True,
    False,
    Extend,
};

struct CoreStratifiedResult
{
    bool stratified = false;
    unsigned numberOfRestrainedGroups = 0;
    unsigned biggestRestrainedGroupSize = 0;
    unsigned numberOfRulesInRestrainedGroups = 0;
};

const size_t rulePairCacheSize = 40000;
struct RuleHashInfo
{
    std::unordered_map<PredId_t, unsigned> predIdToLocal;
    unsigned localPredicate = 0;
    std::string firstRuleString = "";
    std::string secondRuleString = "";
};

struct VariableAssignments
{
    VariableAssignments(unsigned variableCountFrom, unsigned variableCountTo)
    {
        size_t variableCount = variableCountFrom + variableCountTo;
        groupGraph = SimpleGraph(variableCount);

        constantAssignment.resize(variableCount, NOT_ASSIGNED);
        groupAssignments.resize(variableCount, NOT_ASSIGNED);

        variableToOffset = variableCountFrom;
    }

    size_t getVariableToOffset() const { return variableToOffset; }

    int64_t getGroupId(int32_t variableId, RelianceRuleRelation relation) const
    {
        if (relation == RelianceRuleRelation::From)
            return groupAssignments[std::abs(variableId)];
        else 
            return groupAssignments[std::abs(variableId) + variableToOffset];
    }

    int64_t getConstant(int32_t variableId, RelianceRuleRelation relation) const
    {
        if (relation == RelianceRuleRelation::From)
            return constantAssignment[std::abs(variableId)];
        else 
            return constantAssignment[std::abs(variableId) + variableToOffset];
    }

    void connectVariables(int32_t variableIdFrom, int32_t variableIdTo) 
    {
        variableIdFrom = std::abs(variableIdFrom);
        variableIdTo = std::abs(variableIdTo) + variableToOffset;

        groupGraph.addEdge(variableIdFrom, variableIdTo);
        groupGraph.addEdge(variableIdTo, variableIdFrom);

        groupAssignments[variableIdFrom] = ASSIGNED;
        groupAssignments[variableIdTo] = ASSIGNED;

        if (constantAssignment[variableIdFrom] != NOT_ASSIGNED)
        {
            assignConstantsNext(variableIdTo, constantAssignment[variableIdFrom]);
        }
        else if (constantAssignment[variableIdTo] != NOT_ASSIGNED)
        {
            assignConstantsNext(variableIdFrom, constantAssignment[variableIdTo]);
        }
    }

    void connectVariablesSelf(int32_t variableIdFrom, int32_t variableIdTo) 
    {
        variableIdFrom = std::abs(variableIdFrom);
        variableIdTo = std::abs(variableIdTo); //Everything is in From

        groupGraph.addEdge(variableIdFrom, variableIdTo);
        groupGraph.addEdge(variableIdTo, variableIdFrom);

        groupAssignments[variableIdFrom] = ASSIGNED;
        groupAssignments[variableIdTo] = ASSIGNED;

        if (constantAssignment[variableIdFrom] != NOT_ASSIGNED)
        {
            assignConstantsNext(variableIdTo, constantAssignment[variableIdFrom]);
        }
        else if (constantAssignment[variableIdTo] != NOT_ASSIGNED)
        {
            assignConstantsNext(variableIdFrom, constantAssignment[variableIdTo]);
        }
    }

    void assignConstants(int32_t variableId, RelianceRuleRelation relation, int64_t constant)
    {
        variableId = std::abs(variableId);
        int32_t trueId = variableId + ((relation == RelianceRuleRelation::From) ? 0 : variableToOffset);
        assignConstantsNext(trueId, constant);
    }

    void finishGroupAssignments()
    {
        for (int32_t variableId = 0; variableId < groupAssignments.size(); ++variableId)
        {
            if (groupAssignments[variableId] == ASSIGNED)
            {
                finishGroupAssignmentsNext(variableId, currentGroupId);
                ++currentGroupId;
            }
        }
    }

    bool hasMappedExistentialVariable = false;
private:
    size_t variableToOffset;
    int64_t currentGroupId = 0;

    SimpleGraph groupGraph;

    std::vector<int64_t> constantAssignment;
    std::vector<int64_t> groupAssignments;

    void assignConstantsNext(int32_t trueId, int64_t constant)
    {
        constantAssignment[trueId] = constant;

        for (size_t successor : groupGraph.edges[trueId])
        {
            if (constantAssignment[successor] == NOT_ASSIGNED)
            {
                assignConstantsNext((int32_t)successor, constant);
            }
        }
    }

    void finishGroupAssignmentsNext(int32_t trueId, int64_t groupId)
    {
        groupAssignments[trueId] = groupId;

        for (size_t successor : groupGraph.edges[trueId])
        {
            if (groupAssignments[successor] != groupId)
            {
                finishGroupAssignmentsNext((int32_t)successor, groupId);
            }
        }
    }
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

enum class Sat
{
    Unsatisfied,
    Satisfied,
    Existential
};

struct RelianceGroupResult
{
    std::vector<std::vector<unsigned>> groups;
    std::vector<unsigned> assignments;
    // std::vector<bool> hasPredeccessors;
    size_t minimumGroup;
};

// Common
TermInfo getTermInfoUnify(VTerm term, const VariableAssignments &assignments, RelianceRuleRelation relation);
bool termsEqual(const TermInfo &termLeft, const TermInfo &termRight);
unsigned highestLiteralsId(const std::vector<Literal> &literalVector);
bool checkConsistentExistential(const std::vector<std::vector<std::unordered_map<int64_t, TermInfo>>> &mappings);
bool relianceModels(const std::vector<Literal> &left, RelianceRuleRelation leftRelation, const std::vector<Literal> &right, RelianceRuleRelation rightRelation, const VariableAssignments &assignments, std::vector<unsigned> &satisfied, std::vector<std::vector<std::unordered_map<int64_t, TermInfo>>> &existentialMappings, bool alwaysDefaultAssignExistentials = false, bool treatExistentialAsVariables = true);
bool unifyTerms(const TermInfo &fromInfo, const TermInfo &toInfo, VariableAssignments &assignments);
Rule markExistentialVariables(const Rule &rule);
void prepareExistentialMappings(const std::vector<Literal> &right, RelianceRuleRelation rightRelation, const VariableAssignments &assignments, std::vector<std::vector<std::unordered_map<int64_t, TermInfo>>> &existentialMappings);

// For outside
RelianceComputationResult computePositiveReliances(const std::vector<Rule> &rules, RelianceStrategy strat = RelianceStrategy::Full, unsigned timeoutMilliSeconds = 0.0);
RelianceComputationResult computeRestrainReliances(const std::vector<Rule> &rules, RelianceStrategy strat = RelianceStrategy::Full, unsigned timeoutMilliSeconds = 0.0);
unsigned DEBUGcountFakePositiveReliances(const std::vector<Rule> &rules, const SimpleGraph &positiveGraph);
std::pair<SimpleGraph, SimpleGraph> combineGraphs(const SimpleGraph &positiveGraph, const SimpleGraph &restraintGraph);
void splitIntoPieces(const Rule &rule, std::vector<Rule> &outRules);
RelianceGroupResult computeRelianceGroups(const SimpleGraph &graph, const SimpleGraph &graphTransposed, std::vector<bool> *activeNodes = nullptr);
CoreStratifiedResult isCoreStratified(const SimpleGraph & unionGraph, const SimpleGraph & unionGraphTransposed, const SimpleGraph &restrainingGraph);
std::string rulePairHash(RuleHashInfo ruleFromInfo, const RuleHashInfo &ruleToInfo, const Rule &ruleTo);
RuleHashInfo ruleHashInfoFirst(const Rule &rule);
#endif
#ifndef _SEMINAIVER_ORDERED_H
#define _SEMINAIVER_ORDERED_H

#include "vlog/reliances/reliances.h"
#include <vlog/seminaiver.h>

#include <vector>
#include <stack>
#include <unordered_set>
#include <deque>
#include <algorithm>
#include <numeric>

struct UnorderedGraph
{
    std::unordered_map<unsigned, std::unordered_set<unsigned>> edges;

    void addEdge(unsigned from, unsigned to)
    {
        edges[from].insert(to);
    }

    void removeNode(unsigned node)
    {
        edges.erase(node);

        for (auto nodes : edges)
        {
            nodes.second.erase(node);
        }
    }
};

class SemiNaiverOrdered: public SemiNaiver {
public:
    struct PositiveGroup;

    struct RelianceRuleInfo
    {
        unsigned id;

        RuleExecutionDetails *ruleDetails;

        bool active = true;
        bool triggered = false;
        unsigned numRestrains = 0;

        PositiveGroup *positiveGroup;
        std::vector<RelianceRuleInfo*> positiveSuccessors, restraintSuccessors;

        RelianceRuleInfo(unsigned id, RuleExecutionDetails *details)
            : id(id), ruleDetails(details)
        {
            for (RelianceRuleInfo *successor : restraintSuccessors)
            {
                ++successor->numRestrains;
            }
        }

        void initialize(PositiveGroup *group,
            std::vector<RelianceRuleInfo*> positiveSuccessors, std::vector<RelianceRuleInfo*> restraintSuccessors)
        {
            this->positiveGroup = group;
            this->positiveSuccessors = positiveSuccessors;
            this->restraintSuccessors = restraintSuccessors;

            for (RelianceRuleInfo *successor : restraintSuccessors)
            {
                ++successor->numRestrains;
            }
        }

        void setTriggered(bool triggered)
        {
            if (this->triggered && !triggered)
            {
                --positiveGroup->numTriggeredRules;
            }
            else
            {
                ++positiveGroup->numTriggeredRules;
            }

            this->triggered = triggered;
        }

        void setInactive()
        {
            active = false;

            for (RelianceRuleInfo *memberSuccessor : restraintSuccessors)
            {
                --memberSuccessor->numRestrains;
            }
        }
    };

    struct PositiveGroup
    {
        unsigned id;

        unsigned numActivePredecessors = 0;
        unsigned numTriggeredRules = 0;
    
        std::vector<RelianceRuleInfo*> members;
        std::vector<PositiveGroup*> successors;

        bool isActive()
        {
            return numActivePredecessors > 0 || numTriggeredRules > 0;
        }

        void setInactive()
        {
            if (isActive())
            {
                std::cerr << "setInactive on active group?" << std::endl;
                return;
            }

            for (RelianceRuleInfo *memberInfo : members)
            {
                memberInfo->setInactive();
            }
        }
    };

    struct RestrainedGroup
    {
        std::vector<PositiveGroup*> positiveGroups; 
        std::vector<RelianceRuleInfo*> restrainedMembers, unrestrainedMembers;
    };

    struct RelianceGroupResult
    {
        std::vector<std::vector<unsigned>> groups;
        std::vector<unsigned> assignments;
    };

    VLIBEXP SemiNaiverOrdered(EDBLayer &layer,
        Program *program, 
        bool opt_intersect,
        bool opt_filtering, 
        bool multithreaded,
        TypeChase chase, 
        int nthreads, 
        bool shuffleRules,
        bool ignoreExistentialRule,
        Program *RMFC_check = NULL);

    VLIBEXP void run(size_t lastIteration,
        size_t iteration,
        unsigned long *timeout = NULL,
        bool checkCyclicTerms = false,
        int singleRule = -1,
        PredId_t predIgnoreBlock = -1);
        
private:
    template<typename GraphType>
    void fillOrder(GraphType &graph, unsigned node, std::vector<unsigned> &visited, stack<unsigned> &stack);
    template<typename GraphType>
    void dfsUntil(GraphType &graph, unsigned node, std::vector<unsigned> &visited, std::vector<unsigned> &currentGroup);
    template<typename GraphType>
    RelianceGroupResult computeRelianceGroups(GraphType &graph, GraphType &graphTransposed);
    void setActive(PositiveGroup *currentGroup);
    void sortRuleVectorById(std::vector<RelianceRuleInfo *> &infos);
    std::vector<PositiveGroup> SemiNaiverOrdered::computePositiveGroups(std::vector<RelianceRuleInfo> &allRules, SimpleGraph &positiveGraph, RelianceGroupResult &groupsResult);

    void orderGroupExistentialLast(PositiveGroup *group);
    void orderGroupAverageRuntime(PositiveGroup *group);
    void orderGroupPredicateCount(PositiveGroup *group);
    void orderGroupManually(PositiveGroup *group);

    // void prepare(size_t lastExecution, int singleRuleToCheck, const std::vector<Rule> &allRules, const SimpleGraph &positiveGraph, const SimpleGraph &positiveGraphTransposed, const SimpleGraph &blockingGraphTransposed, const RelianceGroupResult &groupsResult, std::vector<PositiveGroup> &positiveGroups);
    void prepare(size_t lastExecution, int singleRuleToCheck, const std::vector<Rule> &allRules, std::vector<RelianceRuleInfo> &outInfo, std::vector<RuleExecutionDetails> &outRuleDetails);

    void updateGraph(UnorderedGraph &graph, UnorderedGraph &graphTransposed, PositiveGroup *group);
    std::pair<UnorderedGraph, UnorderedGraph> combineGraphs(const SimpleGraph &positiveGraph, const SimpleGraph &restraintGraph);

    bool executeGroup(std::vector<RuleExecutionDetails> &ruleset, std::vector<StatIteration> &costRules, bool fixpoint, unsigned long *timeout);
    // bool executeGroupBottomUp(std::vector<RuleExecutionDetails> &ruleset, std::vector<unsigned> &rulesetOrder, std::vector<StatIteration> &costRules, bool blocked, unsigned long *timeout);
    // bool executeGroupInOrder(std::vector<RuleExecutionDetails> &ruleset, std::vector<unsigned> &rulesetOrder, std::vector<StatIteration> &costRules, bool blocked, unsigned long *timeout);
    // bool SemiNaiverOrdered::executeGroupAverageRuntime(std::vector<RuleExecutionDetails> &ruleset, std::vector<StatIteration> &costRules, bool blocked, unsigned long *timeout);
    PositiveGroup *SemiNaiverOrdered::executeRestrainedGroup(RestrainedGroup &group, std::vector<StatIteration> &costRules, unsigned long *timeout);
};

#endif

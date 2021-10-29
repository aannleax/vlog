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

class SemiNaiverOrdered: public SemiNaiver {
public:
    struct PositiveGroup
    {
        std::vector<RuleExecutionDetails> rules;
        std::vector<PositiveGroup *> successors;
        std::vector<PositiveGroup *> predecessors;

        std::vector<PositiveGroup *> blockers;

        bool active = false; //can potentially be executed
        bool inQueue = false; //already in queue
        bool triggered = false; //contains rules which may produce new facts
    
        std::vector<unsigned> order;
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
    void fillOrder(const SimpleGraph &graph, unsigned node, std::vector<unsigned> &visited, stack<unsigned> &stack);
    void dfsUntil(const SimpleGraph &graph, unsigned node, std::vector<unsigned> &visited, std::vector<unsigned> &currentGroup);
    RelianceGroupResult computeRelianceGroups(const SimpleGraph &graph, const SimpleGraph &graphTransposed);
    void setActive(PositiveGroup *currentGroup);

    void orderGroupExistentialLast(PositiveGroup *group);
    void orderGroupAverageRuntime(PositiveGroup *group);
    void orderGroupPredicateCount(PositiveGroup *group);
    void orderGroupManually(PositiveGroup *group);

    void prepare(size_t lastExecution, int singleRuleToCheck, const std::vector<Rule> &allRules, const SimpleGraph &positiveGraph, const SimpleGraph &positiveGraphTransposed, const SimpleGraph &blockingGraphTransposed, const RelianceGroupResult &groupsResult, std::vector<PositiveGroup> &positiveGroups);
  
    bool executeGroup(std::vector<RuleExecutionDetails> &ruleset, std::vector<StatIteration> &costRules, bool fixpoint, unsigned long *timeout);
    bool executeGroupBottomUp(std::vector<RuleExecutionDetails> &ruleset, std::vector<unsigned> &rulesetOrder, std::vector<StatIteration> &costRules, bool blocked, unsigned long *timeout);
    bool executeGroupInOrder(std::vector<RuleExecutionDetails> &ruleset, std::vector<unsigned> &rulesetOrder, std::vector<StatIteration> &costRules, bool blocked, unsigned long *timeout);
    bool SemiNaiverOrdered::executeGroupAverageRuntime(std::vector<RuleExecutionDetails> &ruleset, std::vector<StatIteration> &costRules, bool blocked, unsigned long *timeout);
};

#endif

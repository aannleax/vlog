#ifndef _SEMINAIVER_ORDERED_H
#define _SEMINAIVER_ORDERED_H

#include "vlog/reliances/reliances.h"
#include <vlog/seminaiver.h>

#include <vector>
#include <stack>
#include <unordered_set>
#include <deque>

class SemiNaiverOrdered: public SemiNaiver {
public:
    struct PositiveGroup
    {
        std::vector<RuleExecutionDetails> rules;
        std::vector<PositiveGroup *> successors;
        std::vector<PositiveGroup *> predecessors;

        bool active = true; //can potentially be executed
        bool inQueue = false; //already in queue
        bool triggered = false; //contains rules which may produce new facts
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
    void fillOrder(const RelianceGraph &graph, unsigned node, std::vector<unsigned> &visited, stack<unsigned> &stack);
    void dfsUntil(const RelianceGraph &graph, unsigned node, std::vector<unsigned> &visited, std::vector<unsigned> &currentGroup);
    RelianceGroupResult computeRelianceGroups(const RelianceGraph &graph, const RelianceGraph &graphTransposed);

    void prepare(size_t lastExecution, int singleRuleToCheck, const std::vector<Rule> &allRules, const RelianceGraph &positiveGraph, const RelianceGraph &positiveGraphTransposed, const RelianceGroupResult &groupsResult, std::vector<PositiveGroup> &positiveGroups);
};

#endif

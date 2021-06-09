#ifndef _SEMINAIVER_ORDERED_H
#define _SEMINAIVER_ORDERED_H

#include "vlog/reliances/reliances.h"
#include <vlog/seminaiver.h>

#include <vector>
#include <stack>
#include <unordered_set>
#include <queue>

class SemiNaiverOrdered: public SemiNaiver {
public:
    struct PositiveGroup
    {
        std::vector<RuleExecutionDetails> rules;
        std::vector<PositiveGroup *> successors;

        bool active = true; //can potentially be executed
        bool triggered = true; //already in queue
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
    std::vector<std::vector<unsigned>> computeRelianceGroups(const RelianceGraph &graph, const RelianceGraph &graphTransposed);

    void prepare(size_t lastExecution, int singleRuleToCheck, const std::vector<Rule> &allRules, const RelianceGraph &positiveGraph, const std::vector<std::vector<unsigned>> &groups, std::vector<PositiveGroup> &positiveGroups);
};

#endif

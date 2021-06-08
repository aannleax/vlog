#include "vlog/seminaiver_ordered.h"

#include <iostream>
#include <vector>

SemiNaiverOrdered::SemiNaiverOrdered(EDBLayer &layer,
    Program *program, 
    bool opt_intersect,
    bool opt_filtering, 
    bool multithreaded,
    TypeChase chase, 
    int nthreads, 
    bool shuffleRules,
    bool ignoreExistentialRule,
    Program *RMFC_check) : SemiNaiver(layer,
        program, opt_intersect, opt_filtering, multithreaded, chase,
        nthreads, shuffleRules, ignoreExistentialRule, RMFC_check)
{
    std::cout << "Running constructor of SemiNaiverOrdered" << std::endl;
}

void SemiNaiverOrdered::run(size_t lastIteration,
    size_t iteration,
    unsigned long *timeout,
    bool checkCyclicTerm,
    int singleRule,
    PredId_t predIgnoreBlock)
{
    std::cout << "Rules.tostring():" << std::endl;
    for (Rule &currentRule: program->getAllRules())
    {
        std::cout << currentRule.tostring() << std::endl;
    }

    std::cout << "Computing positive reliances..." << std::endl;
    std::pair<RelianceGraph, RelianceGraph> relianceGraphs = computePositiveReliances(program);

    std::cout << "Positive reliances: " << std::endl;
    for (unsigned from = 0; from < relianceGraphs.first.edges.size(); ++from)
    {
        for (unsigned to :  relianceGraphs.first.edges[from])
        {
            std::cout << "positive reliance: " << from << " -> " << to << std::endl;
        }
    }
}
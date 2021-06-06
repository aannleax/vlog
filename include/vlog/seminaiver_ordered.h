#ifndef _SEMINAIVER_ORDERED_H
#define _SEMINAIVER_ORDERED_H

#include <vlog/seminaiver.h>

#include <vector>

class SemiNaiverOrdered: public SemiNaiver {
    public:
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
        
        /*VLIBEXP void run(unsigned long *timeout = NULL, bool checkCyclicTerms = false) {
            run(0, 1, timeout, checkCyclicTerms, -1, -1);
        }*/

};

#endif

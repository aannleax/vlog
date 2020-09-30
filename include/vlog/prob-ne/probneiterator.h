#ifndef _PROB_NE_ITR_H
#define _PROB_NE_ITR_H

#include <vlog/edbiterator.h>

class ProbNETable;
class ProbNEIterator : public EDBIterator
{
    private:
        const PredId_t predid;
        const uint64_t nterms;
        const ProbNETable *table;
        Term_t v1, v2;

    public:
        ProbNEIterator(PredId_t predid, ProbNETable *table, uint64_t nterms);

        bool hasNext();

        void next();

        void next(Term_t hint1, Term_t hint2);

        Term_t getElementAt(const uint8_t p);

        PredId_t getPredicateID();

        void skipDuplicatedFirstColumn();

        void clear();
};

#endif

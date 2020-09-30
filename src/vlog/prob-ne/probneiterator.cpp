#include <vlog/prob-ne/probneiterator.h>

ProbNEIterator::ProbNEIterator(
        PredId_t predid,
         ProbNETable *table,
        uint64_t  nterms) : predid(predid), table(table), nterms(nterms)
{
    v1 = v2 = 0;
}

bool ProbNEIterator::hasNext()
{
    return true;
}

void ProbNEIterator::next()
{
    LOG(ERRORL) << "ProbNEIterator: Not supported";
    throw 10;
}

void ProbNEIterator::next(Term_t hint1, Term_t hint2)
{
    if (hint1 == hint2) {
        //The values are the same. I return a tuple that is slightly bigger
        //so that the join will fail and I can check the next pair
        v1 = hint1;
        v2 = hint2 + 1;
    } else {
        v1 = hint1;
        v2 = hint2;
    }
}

Term_t ProbNEIterator::getElementAt(const uint8_t p)
{
    assert(p < 2);
    if (p == 0)
        return v1;
    else
        return v2;
}

PredId_t ProbNEIterator::getPredicateID()
{
    return predid;
}

void ProbNEIterator::skipDuplicatedFirstColumn()
{
    LOG(ERRORL) << "ProbNEIterator: Not supported";
    throw 10;
}

void ProbNEIterator::clear()
{
}

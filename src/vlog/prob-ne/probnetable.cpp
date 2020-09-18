#include <vlog/prob-ne/probnetable.h>

ProbNETable::ProbNETable(PredId_t predid, std::string predname)
{
}

void ProbNETable::query(QSQQuery *query, TupleTable *outputTable,
        std::vector<uint8_t> *posToFilter,
        std::vector<Term_t> *valuesToFilter)
{
    LOG(ERRORL) << "ProbNETable: Not supported";
    throw 10;
}

size_t ProbNETable::estimateCardinality(const Literal &query)
{
}

size_t ProbNETable::getCardinality(const Literal &query)
{
}

size_t ProbNETable::getCardinalityColumn(const Literal &query, uint8_t posColumn)
{
}

bool ProbNETable::isEmpty(const Literal &query, std::vector<uint8_t> *posToFilter,
        std::vector<Term_t> *valuesToFilter)
{
}

EDBIterator *ProbNETable::getIterator(const Literal &query)
{
}

EDBIterator *ProbNETable::getSortedIterator(const Literal &query,
        const std::vector<uint8_t> &fields)
{
}

bool ProbNETable::getDictNumber(const char *text, const size_t sizeText,
        uint64_t &id)
{
}

bool ProbNETable::getDictText(const uint64_t id, char *text)
{
}

bool ProbNETable::getDictText(const uint64_t id, std::string &text)
{
}

uint64_t ProbNETable::getNTerms()
{
    LOG(ERRORL) << "ProbNETable: Not supported";
    throw 10;
}

uint64_t ProbNETable::getSize()
{
    LOG(ERRORL) << "ProbNETable: Not supported";
    throw 10;
}

void ProbNETable::releaseIterator(EDBIterator *itr)
{
}

uint8_t ProbNETable::getArity() const
{
    return 2;
}

ProbNETable::~ProbNETable()
{
}

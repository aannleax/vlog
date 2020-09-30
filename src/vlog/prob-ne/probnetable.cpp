#include <vlog/prob-ne/probnetable.h>
#include <vlog/prob-ne/probneiterator.h>
#include <vlog/edbiterator.h>

ProbNETable::ProbNETable(PredId_t predid, std::string predname) :
    predid(predid),
    predname(predname)
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
    return getCardinality(query);
}

size_t ProbNETable::getCardinality(const Literal &query)
{
    return (size_t) - 1;
}

size_t ProbNETable::getCardinalityColumn(const Literal &query, uint8_t posColumn)
{
    LOG(ERRORL) << "ProbNETable: Not supported";
    throw 10;
}

bool ProbNETable::isEmpty(const Literal &query, std::vector<uint8_t> *posToFilter,
        std::vector<Term_t> *valuesToFilter)
{
    if (query.getNConstants() != 0 ||
            query.getTupleSize() != query.getNUniqueVars()) {
        LOG(ERRORL) << "ProbNETable: Not supported";
        throw 10;
    }
    return false;
}

EDBIterator *ProbNETable::getIterator(const Literal &query)
{
    std::vector<uint8_t> fields;
    return getSortedIterator(query, fields);
}

EDBIterator *ProbNETable::getSortedIterator(const Literal &query,
        const std::vector<uint8_t> &fields)
{
    return new ProbNEIterator(predid, this, ~0ul);
}

bool ProbNETable::getDictNumber(const char *text, const size_t sizeText,
        uint64_t &id)
{
    LOG(ERRORL) << "ProbNETable: Not supported";
    throw 10;
}

bool ProbNETable::getDictText(const uint64_t id, char *text)
{
    LOG(ERRORL) << "ProbNETable: Not supported";
    throw 10;
}

bool ProbNETable::getDictText(const uint64_t id, std::string &text)
{
    LOG(ERRORL) << "ProbNETable: Not supported";
    throw 10;
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
    delete itr;
}

uint8_t ProbNETable::getArity() const
{
    return 2;
}

bool ProbNETable::expensiveLayer()
{
    return true;
}

ProbNETable::~ProbNETable()
{
}

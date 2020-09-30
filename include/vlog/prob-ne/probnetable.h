#ifndef _PROB_NE_H
#define _PROB_NE_H

#include <vlog/concepts.h>
#include <vlog/edbtable.h>

#include <vector>

class ProbNETable: public EDBTable
{
    private:
        PredId_t predid;
        std::string predname;

    public:
        ProbNETable(PredId_t predid, std::string predname);

        void query(QSQQuery *query, TupleTable *outputTable,
                std::vector<uint8_t> *posToFilter,
                std::vector<Term_t> *valuesToFilter);

        size_t estimateCardinality(const Literal &query);

        size_t getCardinality(const Literal &query);

        size_t getCardinalityColumn(const Literal &query, uint8_t posColumn);

        bool isEmpty(const Literal &query, std::vector<uint8_t> *posToFilter,
                std::vector<Term_t> *valuesToFilter);

        EDBIterator *getIterator(const Literal &query);

        EDBIterator *getSortedIterator(const Literal &query,
                const std::vector<uint8_t> &fields);

        bool getDictNumber(const char *text, const size_t sizeText,
                uint64_t &id);

        bool getDictText(const uint64_t id, char *text);

        bool getDictText(const uint64_t id, std::string &text);

        uint64_t getNTerms();

        uint64_t getSize();

        void releaseIterator(EDBIterator *itr);

        bool expensiveLayer();

        uint8_t getArity() const;

        ~ProbNETable();
};

#endif

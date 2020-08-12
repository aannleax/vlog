#ifndef _FUNCT_EXT_RESULT_JOIN_PROCESSOR_H
#define _FUNCT_EXT_RESULT_JOIN_PROCESSOR_H

#include <vlog/extresultjoinproc.h>

class FunctExtRuleProcessor : public ExistentialRuleProcessor {
    private:
        std::vector<std::pair<uint8_t, uint8_t>> newPosFromSecond;
        const std::vector<std::pair<Var_t, FunctorIdAndPos_t>> *functors;

        std::vector<std::pair<uint8_t, uint8_t>> &expandToFunctors(
                std::vector<std::pair<uint8_t, uint8_t>> &posFromSecond,
                const RuleExecutionDetails *detailsRule,
                std::vector<Literal> &heads);

    public:
        FunctExtRuleProcessor(
                std::vector<std::pair<uint8_t, uint8_t>> &posFromFirst,
                std::vector<std::pair<uint8_t, uint8_t>> &posFromSecond,
                std::vector<FCBlock> &listDerivations,
                std::vector<Literal> &heads,
                const RuleExecutionDetails *detailsRule,
                const uint8_t ruleExecOrder,
                const size_t iteration,
                const bool addToEndTable,
                const int nthreads,
                SemiNaiver *sn,
                std::shared_ptr<ChaseMgmt> chaseMgmt,
                bool filterRecursive,
                const bool ignoreDupElimin);

        void addColumns(const int blockid, FCInternalTableItr *itr,
                const bool unique, const bool sorted,
                const bool lastInsert);

        void addColumns(const int blockid,
                std::vector<std::shared_ptr<Column>> &columns,
                const bool unique, const bool sorted) {
            LOG(ERRORL) << "Not implemented";
            throw 10;
        }

        void addColumn(const int blockid, const uint8_t pos,
                std::shared_ptr<Column> column, const bool unique,
                const bool sorted) {
            LOG(ERRORL) << "Not implemented";
            throw 10;
        }

        void processResults(const int blockid, const Term_t *first,
                FCInternalTableItr* second, const bool unique) {
            LOG(ERRORL) << "Not implemented";
            throw 10;
        }

        void processResults(const int blockid,
                const std::vector<const std::vector<Term_t> *> &vectors1, size_t i1,
                const std::vector<const std::vector<Term_t> *> &vectors2, size_t i2,
                const bool unique) {
            LOG(ERRORL) << "Not implemented";
            throw 10;
        }

        void processResults(std::vector<int> &blockid, Term_t *p,
                std::vector<bool> &unique, std::mutex *m) {
            LOG(ERRORL) << "Not implemented";
            throw 10;
        }

        void processResults(const int blockid, FCInternalTableItr *first,
                FCInternalTableItr* second, const bool unique) {
            LOG(ERRORL) << "Not implemented";
            throw 10;
        }
};

#endif

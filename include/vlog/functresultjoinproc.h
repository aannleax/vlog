#ifndef _FUNCT_RESULT_JOIN_PROCESSOR_H
#define _FUNCT_RESULT_JOIN_PROCESSOR_H

#include <vlog/finalresultjoinproc.h>
#include <vlog/functormap.h>

class FunctRuleProcessor : public SingleHeadFinalRuleProcessor {
    private:
        const std::vector<std::pair<Var_t, FunctorIdAndPos_t>> *functors;
        //const RuleExecutionPlan *plan;
        const size_t nOldCopyFromSecond;
        FunctorMap &functorMap;
        std::unique_ptr<uint64_t[]> functorArgs;

        void expandToFunctors(
                const RuleExecutionPlan *plan,
                Literal &head);

    public:
        FunctRuleProcessor(
                SemiNaiver *sn,
                std::vector<std::pair<uint8_t, uint8_t>> &posFromFirst,
                std::vector<std::pair<uint8_t, uint8_t>> &posFromSecond,
                std::vector<FCBlock> &listDerivations,
                FCTable *t,
                Literal &head,
                const uint8_t posHeadInRule,
                const RuleExecutionDetails *detailsRule,
                const uint8_t ruleExecOrder,
                const RuleExecutionPlan *plan,
                const size_t iteration,
                const bool addToEndTable,
                const int nthreads,
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
                FCInternalTableItr* second, const bool unique);

        void processResults(const int blockid,
                const std::vector<const std::vector<Term_t> *> &vectors1, size_t i1,
                const std::vector<const std::vector<Term_t> *> &vectors2, size_t i2,
                const bool unique);

        void processResults(std::vector<int> &blockid, Term_t *p,
                std::vector<bool> &unique, std::mutex *m) {
            LOG(ERRORL) << "Not implemented";
            throw 10;
        }

        void processResults(const int blockid, FCInternalTableItr *first,
                FCInternalTableItr* second, const bool unique);
};

#endif

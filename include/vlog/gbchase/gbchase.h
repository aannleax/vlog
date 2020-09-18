#ifndef _GB_CHASE_H
#define _GB_CHASE_H

#include <vlog/concepts.h>
#include <vlog/chase.h>
#include <vlog/edb.h>
#include <vlog/fcinttable.h>

#include <vlog/gbchase/gbsegment.h>
#include <vlog/gbchase/gbgraph.h>
#include <vlog/gbchase/gbruleexecutor.h>

#include <chrono>

typedef enum { GBCHASE, TGCHASE_STATIC, TGCHASE_DYNAMIC } GBChaseAlgorithm;

class GBChase : public Chase {
    protected:
        const bool trackProvenance;
        const bool filterQueryCont;
        const bool edbCheck;
        Program *program;
        EDBLayer &layer; //Stores the input data
        GBGraph g; //Stores the derivations
        std::vector<Rule> rules;

    private:
        std::vector<int> stratification;
        int nStratificationClasses;
        GBRuleExecutor executor; //Object that executes rules
        size_t currentIteration;

        PredId_t currentPredicate;
#ifdef WEBINTERFACE
        std::string currentRule;
#endif
        std::map<PredId_t, std::shared_ptr<FCTable>> cacheFCTables;

        //Used for statistics
        size_t triggers;

        void createNewNodesWithProv(
                size_t ruleIdx, size_t step,
                std::shared_ptr<const TGSegment> seg,
                std::vector<std::shared_ptr<Column>> &provenance);

    protected:
        std::pair<bool, size_t> determineAdmissibleRule(
                const size_t &ruleIdx,
                const size_t stratumLevel,
                const size_t stepStratum,
                const size_t step) const;

        void determineAdmissibleRules(
                const std::vector<size_t> &ruleIdxs,
                const size_t stratumLevel,
                const size_t stepStratum,
                const size_t step,
                std::vector<std::pair<size_t,size_t>> &admissibleRules) const;

        void prepareRuleExecutionPlans(
                const size_t &ruleIdx,
                const size_t prevstep,
                const size_t step,
                std::vector<GBRuleInput> &newnodes);

        bool executeRule(GBRuleInput &node, bool cleanDuplicates = true);

        virtual size_t executeRulesInStratum(
                const std::vector<size_t> &ruleIdxs,
                const size_t stratumLevel,
                const size_t stepStratum,
                size_t &step);

    public:
        VLIBEXP GBChase(EDBLayer &layer, Program *program,
                bool useCacheRetain = true,
                bool trackProvenance = false,
                bool filterQueryCont = false,
                bool edbCheck = false,
                bool rewriteCliques = false);

        VLIBEXP virtual void run();

        Program *getProgram();

        EDBLayer &getEDBLayer();

        size_t getSizeTable(const PredId_t predid) const;

        FCIterator getTableItr(const PredId_t predid);

        FCTable *getTable(const PredId_t predid);

        size_t getCurrentIteration();

        size_t getNDerivedFacts();

        size_t getNnodes();

        size_t getNedges();

        size_t getNTriggers();

#ifdef WEBINTERFACE
        std::string getCurrentRule();

        PredId_t getCurrentPredicate();
#endif

};

#endif
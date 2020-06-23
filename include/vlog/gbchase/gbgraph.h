#ifndef _GB_GRAPH_H
#define _GB_GRAPH_H

#include <vlog/concepts.h>
#include <vlog/chasemgmt.h>

#include <vlog/gbchase/gbsegment.h>

#include <map>

class GBGraph {
    private:
        struct GBGraph_Node {
            private:
                std::shared_ptr<const TGSegment> data;
            public:
                PredId_t predid;
                size_t ruleIdx;
                size_t step;
                std::vector<size_t> incomingEdges;

                std::unique_ptr<Literal> queryHead; //Used only for query containment
                std::vector<Literal> queryBody; //Used only for query containment
                std::vector<size_t> rangeQueryBody; //Record the boundaries
                //between the rewritings

                GBGraph_Node() : ruleIdx(0), step(0) {}

                std::shared_ptr<const TGSegment> getData() const {
                    return data;
                }

                void setData(std::shared_ptr<const TGSegment> data) {
                    this->data = data;
                }
        };

        struct CacheRetainEntry {
            size_t nnodes;
            std::shared_ptr<const TGSegment> seg;
        };

        const bool trackProvenance;
        const bool cacheRetainEnabled;
        const bool queryContEnabled;

        std::map<PredId_t, std::vector<size_t>> pred2Nodes;
        std::vector<GBGraph_Node> nodes;
        std::map<uint64_t, GBGraph_Node> mapTmpNodes;
        std::map<PredId_t, CacheRetainEntry> cacheRetain;

        //Counter variables
        uint64_t counterNullValues;
        uint32_t counterFreshVarsQueryCont;
        uint64_t counterTmpNodes;
        uint64_t startCounterTmpNodes;

        Rule *allRules;
        EDBLayer *layer;
        Program *program;

        std::chrono::duration<double, std::milli> durationRetain;
        std::chrono::duration<double, std::milli> durationQueryContain;

        std::shared_ptr<const TGSegment> retainVsNodeFast(
                std::shared_ptr<const TGSegment> existuples,
                std::shared_ptr<const TGSegment> newtuples);

        std::shared_ptr<const TGSegment> retainVsNodeFast_one(
                std::shared_ptr<const TGSegment> existuples,
                std::shared_ptr<const TGSegment> newtuples);

        std::shared_ptr<const TGSegment> retainVsNodeFast_two(
                std::shared_ptr<const TGSegment> existuples,
                std::shared_ptr<const TGSegment> newtuples);

        std::shared_ptr<const TGSegment> retainVsNodeFast_generic(
                std::shared_ptr<const TGSegment> existuples,
                std::shared_ptr<const TGSegment> newtuples);

        bool isRedundant_checkTypeAtoms(const std::vector<Literal> &atoms);

        bool isRedundant_checkEquivalenceEDBAtoms(
                std::vector<size_t> &bodyNodeIdxs,
                const Literal &originalRuleHead,
                const std::vector<Literal> &originalRuleBody,
                const Literal *rewrittenRuleHead,
                const std::vector<Literal> &rewrittenRuleBody,
                const std::vector<size_t> &rangeRewrittenRuleBody,
                const size_t nodeId);

        bool isRedundant_checkEquivalenceEDBAtoms_one(
                std::vector<size_t> &bodyNodeIdxs,
                const Literal &originalRuleHead,
                const std::vector<Literal> &originalRuleBody,
                const Literal *rewrittenRuleHead,
                const std::vector<Literal> &rewrittenRuleBody,
                const std::vector<size_t> &rangeRewrittenRuleBody,
                const size_t nodeId);

        bool isRedundant_checkEquivalenceEDBAtoms_two(
                std::vector<size_t> &bodyNodeIdxs,
                const Literal &originalRuleHead,
                const std::vector<Literal> &originalRuleBody,
                const Literal *rewrittenRuleHead,
                const std::vector<Literal> &rewrittenRuleBody,
                const std::vector<size_t> &rangeRewrittenRuleBody,
                const size_t nodeId);

        std::unique_ptr<Literal> createQueryFromNode(
                std::vector<Literal> &outputQueryBody,
                std::vector<size_t> &rangeOutputQueryBody,
                const Rule &rule,
                const std::vector<size_t> &incomingEdges,
                bool incrementCounter = true);

        void addNode(PredId_t predId,
                size_t ruleIdx,
                size_t step,
                std::shared_ptr<const TGSegment> data,
                const std::vector<size_t> &incomingEdges,
                std::unique_ptr<Literal> outputQueryHead,
                std::vector<Literal> &outputQueryBody);

        uint64_t addTmpNode(PredId_t predId,
                std::shared_ptr<const TGSegment> data);

        const GBGraph_Node &getNode(size_t nodeId) const {
            if (trackProvenance && nodeId >= startCounterTmpNodes) {
                assert(mapTmpNodes.count(nodeId));
                return mapTmpNodes.find(nodeId)->second;
            } else {
                assert(nodeId < nodes.size());
                return nodes[nodeId];
            }
        }

    public:
        GBGraph(bool trackProvenance,
                bool cacheRetainEnabled,
                bool useQueryContainmentForRedundancyElim = false) :
            trackProvenance(trackProvenance),
            cacheRetainEnabled(cacheRetainEnabled),
            queryContEnabled(useQueryContainmentForRedundancyElim),
            durationRetain(0),
            durationQueryContain(0), allRules(NULL),
            layer(NULL), program(NULL) {
                counterNullValues = RULE_SHIFT(1);
                counterFreshVarsQueryCont = 1 << 20;
                counterTmpNodes = 1ul << 40;
                startCounterTmpNodes = counterTmpNodes;
            }

        size_t getNNodes() const {
            return nodes.size();
        }

        bool isTmpNode(size_t nodeId) const {
            return nodeId >= startCounterTmpNodes;
        }

        size_t getNodeStep(size_t nodeId) const {
            return getNode(nodeId).step;
        }

        void setRulesProgramLayer(Rule *allRules,
                Program *program,
                EDBLayer *layer) {
            this->allRules = allRules;
            this->program = program;
            this->layer = layer;
        }

        size_t getNodeSize(size_t nodeId) const {
            return getNode(nodeId).getData()->getNRows();
        }

        std::shared_ptr<const TGSegment> getNodeData(size_t nodeId) const {
            return getNode(nodeId).getData();
        }

        PredId_t getNodePredicate(size_t nodeId) const {
            return getNode(nodeId).predid;
        }

        const Literal &getNodeHeadQuery(size_t nodeId) const {
            assert(getNode(nodeId).queryHead.get() != NULL);
            return *(getNode(nodeId).queryHead.get());
        }

        const std::vector<Literal> &getNodeBodyQuery(size_t nodeId) const {
            return getNode(nodeId).queryBody;
        }

        bool areNodesWithPredicate(PredId_t predId) const {
            return pred2Nodes.count(predId);
        }

        const std::vector<size_t> &getNodeIDsWithPredicate(
                PredId_t predId) const {
            return pred2Nodes.at(predId);
        }

        std::shared_ptr<const TGSegment> mergeNodes(
                const std::vector<size_t> &nodeIdxs,
                std::vector<int> &copyVarPos) const;

        void addNodeNoProv(PredId_t predId,
                size_t ruleIdx,
                size_t step,
                std::shared_ptr<const TGSegment> data);

        void addNodeProv(PredId_t predId,
                size_t ruleIdx,
                size_t step,
                std::shared_ptr<const TGSegment> data,
                const std::vector<size_t> &incomingEdges);

        void replaceEqualTerms(
                size_t ruleIdx,
                size_t step,
                std::shared_ptr<const TGSegment> data);

        size_t getNDerivedFacts() const {
            size_t nderived = 0;
            for(auto &node : nodes) {
                nderived += node.getData()->getNRows();
            }
            return nderived;
        }

        uint64_t getCounterNullValues() const {
            return counterNullValues;
        }

        void setCounterNullValues(uint64_t c) {
            counterNullValues = c;
        }

        void cleanTmpNodes() {
            mapTmpNodes.clear();
        }

        std::shared_ptr<const TGSegment> retain(
                PredId_t pred,
                std::shared_ptr<const TGSegment> newtuples);

        //Returns the number of retained tuples. The new node will get the last
        //step and will be assigned to rule ~0ul
        uint64_t mergeNodesWithPredicateIntoOne(PredId_t predId);

        bool isRedundant(size_t ruleIdx,
                std::vector<size_t> &bodyNodeIdx);

        void printStats() {
            LOG(INFOL) << "Time retain (ms): " << durationRetain.count();
        }
};

#endif
#ifndef _CHASE_MGMT_H
#define _CHASE_MGMT_H

#include <vlog/column.h>
#include <vlog/ruleexecdetails.h>

#include <vector>
#include <map>
#include <unordered_map>

#define SIZE_BLOCK 1000

struct ChaseRow {
    uint8_t sz;
    uint64_t *row;

    ChaseRow() : sz(0), row(NULL) {}

    ChaseRow(const uint8_t s, uint64_t *r) : sz(s), row(r) {}

    bool operator == (const ChaseRow &other) const {
        for (int i = 0; i < sz; i++) {
            if (row[i] != other.row[i]) {
                return false;
            }
        }
        return true;
    }
};

struct hash_ChaseRow {
    size_t operator() (const ChaseRow &x) const {
        uint64_t result = 0;
        for (int i = 0; i < x.sz; i++) {
            result = (result + (324723947 + x.row[i])) ^93485734985;
        }
        return (size_t) result;
    }
};

class ChaseMgmt {
    private:
        class Rows {
            private:
                const uint64_t startCounter;
                const uint8_t sizerow;
                uint64_t currentcounter;
                std::vector<std::unique_ptr<uint64_t>> blocks;
                uint32_t blockCounter;
                uint64_t *currentblock;
                std::unordered_map<ChaseRow, uint64_t, hash_ChaseRow> rows;
                bool cyclicTerms;

            public:
                Rows(uint64_t startCounter, uint8_t sizerow) :
                    startCounter(startCounter), sizerow(sizerow) {
                        blockCounter = 0;
                        currentblock = NULL;
                        currentcounter = startCounter;
                        cyclicTerms = false;
                    }

                uint8_t getSizeRow() {
                    return sizerow;
                }

                bool containsCyclicTerms() {
                    return cyclicTerms;
                }

                uint64_t addRow(uint64_t* row);

                bool existingRow(uint64_t *row, uint64_t &value);

                bool checkRecursive(uint64_t target, uint64_t value, std::vector<uint64_t> &toCheck);
        };

        class RuleContainer {
            private:
                std::map<uint8_t, std::vector<uint8_t>> dependencies;
                std::map<uint8_t, ChaseMgmt::Rows> vars2rows;
                uint64_t ruleBaseCounter;
            public:
                RuleContainer(uint64_t ruleBaseCounter,
                        std::map<uint8_t, std::vector<uint8_t>> dep) {
                    this->ruleBaseCounter = ruleBaseCounter;
                    dependencies = dep;
                }

                bool containsCyclicTerms();

                ChaseMgmt::Rows *getRows(uint8_t var);
        };

        std::vector<std::unique_ptr<ChaseMgmt::RuleContainer>> rules;
        const bool restricted;
        const bool checkCyclic;
        const int ruleToCheck;
        bool cyclic;

        bool checkRecursive(uint64_t target, uint64_t rv, int level);

        bool checkNestedRecursive(uint64_t target, uint64_t rv, int level);

    public:
        ChaseMgmt(std::vector<RuleExecutionDetails> &rules,
                const bool restricted, const bool checkCyclic, const int ruleToCheck = -1);

        std::shared_ptr<Column> getNewOrExistingIDs(
                uint32_t ruleid,
                uint8_t var,
                std::vector<std::shared_ptr<Column>> &columns,
                uint64_t size);

        bool checkCyclicTerms(uint32_t ruleid);

        bool checkRecursive(uint64_t rv);

        bool isRestricted() {
            return restricted;
        }

        bool hasRuleToCheck() {
            return ruleToCheck >= 0;
        }

        bool isCheckCyclicMode() {
            return checkCyclic;
        }
};

#endif

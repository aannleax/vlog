#include <vlog/functextresultjoinproc.h>

FunctExtRuleProcessor::FunctExtRuleProcessor(
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
        const bool ignoreDupElimin) :
    ExistentialRuleProcessor(posFromFirst,
            expandToFunctors(posFromSecond, detailsRule, heads),
            listDerivations, heads,detailsRule,
            ruleExecOrder, iteration, addToEndTable,
            nthreads, sn, chaseMgmt, filterRecursive, ignoreDupElimin) {
        functors = &(detailsRule->orderExecutions[ruleExecOrder].
                functvars2posFromSecond);
    }

std::vector<std::pair<uint8_t, uint8_t>> &FunctExtRuleProcessor::expandToFunctors(
        std::vector<std::pair<uint8_t, uint8_t>> &posFromSecond,
        const RuleExecutionDetails *detailsRule,
        std::vector<Literal> &heads) {
    const auto &plan = detailsRule->orderExecutions[ruleExecOrder];
    newPosFromSecond = posFromSecond;
    //Extend it by copying the variables that represent functors
    auto &functList = plan.functvars2posFromSecond;
    size_t i = plan.plan.back()->getTupleSize();
    size_t count = 0;
    size_t countFunctors = 0;
    assert(functList.size() > 0);
    for(const auto head : heads) {
        for(uint8_t j = 0; j < head.getTupleSize(); ++j) {
            if (head.getTermAtPos(j).isVariable()) {
                //Search if the variable is a functor
                auto var = head.getTermAtPos(j).getId();
                if (var == functList[countFunctors].first) {
                    newPosFromSecond.push_back(std::make_pair(count + j, i));
                    countFunctors++;
                    i++;
                    if (countFunctors == functList.size()) {
                        break;
                    }
                }
            }
        }
        count += head.getTupleSize();
    }
    assert(countFunctors == functList.size()); //I've processed all the functors
    return newPosFromSecond;
}

void FunctExtRuleProcessor::addColumns(const int blockid, FCInternalTableItr *itr,
        const bool unique, const bool sorted,
        const bool lastInsert)
{
    if (nCopyFromFirst > 0) {
        LOG(ERRORL) << "This method is not supposed to work if other columns"
            " from outside the itr";
        throw 10;
    }

    std::vector<std::shared_ptr<Column>> c = itr->getAllColumns();
    uint64_t sizecolumns = 0;
    if (c.size() > 0) {
        sizecolumns = c[0]->size();
    }

    if (sizecolumns == 0) {
        return;
    }

    assert(lastInsert); //I'm not sure I handle the case where lastInsert=false
    //Must add columns for the functors
    for(const auto &f : *functors) {
        //TODO
        //For now, create a constant column with dummy IDs
        std::shared_ptr<Column> fc = std::shared_ptr<Column>(new
                CompressedColumn(10, sizecolumns));
        c.push_back(fc);
    }
    addColumns_protected(blockid, c, unique, sorted);
}

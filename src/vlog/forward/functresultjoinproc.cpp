#include <vlog/functresultjoinproc.h>
#include <vlog/seminaiver.h>

FunctRuleProcessor::FunctRuleProcessor(
        SemiNaiver *sn,
        std::vector<std::pair<uint8_t, uint8_t>> &posFromFirst,
        std::vector<std::pair<uint8_t, uint8_t>> &posFromSecond,
        std::vector<FCBlock> &listDerivations,
        FCTable *t,
        Literal &head,
        const uint8_t posHeadInRule,
        const RuleExecutionDetails *detailsRule,
        const uint8_t ruleExecOrder,
        const size_t iteration,
        const bool addToEndTable,
        const int nthreads,
        const bool ignoreDupElimin) :
    SingleHeadFinalRuleProcessor(posFromFirst,
            expandToFunctors(posFromSecond, detailsRule, ruleExecOrder, head),
            listDerivations,
            t,
            head,
            posHeadInRule,
            detailsRule,
            ruleExecOrder,
            iteration,
            addToEndTable,
            nthreads,
            ignoreDupElimin),
    nOldCopyFromSecond(posFromSecond.size()),
    functorMap(sn->getFunctorMap()) {
        //plan = &(detailsRule->orderExecutions[ruleExecOrder]);
        functors = &(detailsRule->orderExecutions[ruleExecOrder].
                functvars2posFromSecond);
        functorArgs = std::unique_ptr<uint64_t[]>(
                new uint64_t[MAX_FUNCTOR_NARGS]);
        assert(functors->size() > 0);
    }

std::vector<std::pair<uint8_t, uint8_t>> &FunctRuleProcessor::expandToFunctors(
        std::vector<std::pair<uint8_t, uint8_t>> &posFromSecond,
        const RuleExecutionDetails *detailsRule,
        const uint8_t ruleExecOrder,
        Literal &head) {
    const auto &plan = detailsRule->orderExecutions[ruleExecOrder];
    newPosFromSecond = posFromSecond;
    //Extend it by copying the variables that represent functors
    auto &functList = plan.functvars2posFromSecond;
    size_t i = plan.plan.back()->getTupleSize();
    size_t countFunctors = 0;
    assert(functList.size() > 0);
    for(uint8_t j = 0; j < head.getTupleSize(); ++j) {
        if (head.getTermAtPos(j).isVariable()) {
            //Search if the variable is a functor
            auto var = head.getTermAtPos(j).getId();
            if (var == functList[countFunctors].first) {
                newPosFromSecond.push_back(std::make_pair(j, i));
                countFunctors++;
                i++;
                if (countFunctors == functList.size()) {
                    break;
                }
            }
        }
    }
    assert(countFunctors == functList.size()); //I've processed all the functors
    return newPosFromSecond;
}

bool __sortByHeadPos(const std::pair<std::shared_ptr<Column>, uint8_t> &a,
        const std::pair<std::shared_ptr<Column>, uint8_t> &b) {
    return a.second < b.second;
}

void FunctRuleProcessor::addColumns(const int blockid, FCInternalTableItr *itr,
        const bool unique, const bool sorted,
        const bool lastInsert)
{
    if (nCopyFromFirst > 0) {
        LOG(ERRORL) << "This method is not supposed to work if other columns"
            " from outside the itr";
        throw 10;
    }

    uint8_t columns[256];
    for (uint32_t i = 0; i < nOldCopyFromSecond; ++i) {
        columns[i] = posFromSecond[i].second;
    }
    std::vector<std::shared_ptr<Column>> c =
        itr->getColumn(nOldCopyFromSecond,
                columns);
    if (rowsize == 0) {
        assert(c.size() == 0);
        return;
    }
    uint64_t sizecolumns = 0;
    if (c.size() > 0) {
        sizecolumns = c[0]->size();
    }

    if (sizecolumns == 0) {
        return;
    }

    //This list does not contain columns in 'c' that are only used to compute
    //functors
    std::vector<std::pair<std::shared_ptr<Column>, uint8_t>> colAndposInHead;
    for (uint32_t i = 0; i < nOldCopyFromSecond; ++i) {
        if (posFromSecond[i].first != ((uint8_t) - 1)) {
            colAndposInHead.push_back(
                    std::make_pair(c[i], posFromSecond[i].first));
        }
    }

    assert(lastInsert); //I'm not sure I handle the case where lastInsert=false
    //Must add columns for the functors
    size_t j = nOldCopyFromSecond;
    for(const auto &f : *functors) {
        ColumnWriter w;
        //Create the functors
        auto fId = f.second.fId;
        std::vector<std::unique_ptr<ColumnReader>> readers;
        size_t nargs = f.second.pos.size();
        for(auto pos : f.second.pos) {
            readers.push_back(c[pos]->getReader());
        }
        //Go through all the values
        for(size_t i = 0; i < sizecolumns; ++i) {
            for(size_t j = 0; j < nargs; ++j) {
                if (!readers[j]->hasNext()) {
                    LOG(ERRORL) << "This should not happen";
                    throw 10;
                }
                functorArgs[j] = readers[j]->next();
            }
            //Get the ID
            uint64_t id = functorMap.getID(fId, functorArgs.get());
            w.add(id);
        }
        colAndposInHead.push_back(
                    std::make_pair(w.getColumn(), posFromSecond[j++].first));
    }
    std::sort(colAndposInHead.begin(), colAndposInHead.end(), __sortByHeadPos);

    //Reorder the columns depending on the position in the head atom
    std::vector<std::shared_ptr<Column>> final_c;
    for(auto &p : colAndposInHead) {
        final_c.push_back(p.first);
    }
    addColumns_protected(blockid, final_c, unique, sorted, lastInsert);
}

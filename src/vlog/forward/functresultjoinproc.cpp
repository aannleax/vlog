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
        const RuleExecutionPlan *plan,
        const size_t iteration,
        const bool addToEndTable,
        const int nthreads,
        const bool ignoreDupElimin) :
    SingleHeadFinalRuleProcessor(posFromFirst,
            posFromSecond,
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
        expandToFunctors(plan, head);
        functors = &(plan->functvars2posFromSecond);
        functorArgs = std::unique_ptr<uint64_t[]>(
                new uint64_t[MAX_FUNCTOR_NARGS]);
        assert(functors->size() > 0);
    }

void FunctRuleProcessor::expandToFunctors(
        const RuleExecutionPlan *plan,
        Literal &head) {
    //Extend it by copying the variables that represent functors
    auto &functList = plan->functvars2posFromSecond;
    size_t i = plan->plan.back()->getTupleSize();
    size_t countFunctors = 0;
    assert(functList.size() > 0);
    for(uint8_t j = 0; j < head.getTupleSize(); ++j) {
        if (head.getTermAtPos(j).isVariable()) {
            //Search if the variable is a functor
            auto var = head.getTermAtPos(j).getId();
            if (var == functList[countFunctors].first) {
                //newPosFromSecond.push_back(std::make_pair(j, i));
                posFromSecond[nCopyFromSecond++] = std::make_pair(j, i);
                countFunctors++;
                i++;
                if (countFunctors == functList.size()) {
                    break;
                }
            }
        }
    }
    assert(countFunctors == functList.size()); //I've processed all the functors
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

    //    uint8_t columns[256];
    //    for (uint32_t i = 0; i < nOldCopyFromSecond; ++i) {
    //        columns[i] = posFromSecond[i].second;
    //    }
    //    std::vector<std::shared_ptr<Column>> c =
    //        itr->getColumn(nOldCopyFromSecond,
    //                columns);
    std::vector<std::shared_ptr<Column>> allCols = itr->getAllColumns();
    if (rowsize == 0) {
        assert(allCols.size() == 0);
        return;
    }
    uint64_t sizecolumns = 0;
    if (allCols.size() > 0) {
        sizecolumns = allCols[0]->size();
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
                    std::make_pair(allCols[posFromSecond[i].second],
                        posFromSecond[i].first));
        }
    }

    //Must add columns for the functors
    size_t j = nOldCopyFromSecond;
    for(const auto &f : *functors) {
        ColumnWriter w;
        //Create the functors
        auto fid = f.second.fId;
        std::vector<std::unique_ptr<ColumnReader>> readers;
        size_t nargs = f.second.pos.size();
        for(auto pos : f.second.pos) {
            readers.push_back(allCols[pos]->getReader());
        }
        //go through all the values
        for(size_t i = 0; i < sizecolumns; ++i) {
            for(size_t j = 0; j < nargs; ++j) {
                if (!readers[j]->hasNext()) {
                    LOG(ERRORL) << "this should not happen";
                    throw 10;
                }
                functorArgs[j] = readers[j]->next();
            }
            //get the id
            uint64_t id = functorMap.getID(fid, functorArgs.get());
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

void FunctRuleProcessor::processResults(const int blockid, FCInternalTableItr *first,
        FCInternalTableItr* second, const bool unique) {
    for (uint32_t i = 0; i < nCopyFromFirst; ++i) {
        if (posFromFirst[i].first != ((uint8_t) - 1)) {
            row[posFromFirst[i].first] = first->getCurrentValue(posFromFirst[i].second);
        }
    }
    for (uint32_t i = 0; i < nCopyFromSecond; ++i) {
        if (posFromSecond[i].first != ((uint8_t) - 1)) {
            if (i < nOldCopyFromSecond) {
                row[posFromSecond[i].first] =
                    second->getCurrentValue(posFromSecond[i].second);
            } else {
                //This is a position added by this object. It represents a functor
                auto &f = functors->at(i - nOldCopyFromSecond);
                auto fid = f.second.fId;
                size_t nargs = f.second.pos.size();
                //go through all the values
                for(size_t j = 0; j < nargs; ++j) {
                    auto pos = f.second.pos[j];
                    //Should I pick it from the left or the right side?
                    if (pos < nCopyFromFirst) {
                        functorArgs[j] = first->getCurrentValue(pos);
                    } else {
                        functorArgs[j] = second->getCurrentValue(
                                pos - nCopyFromFirst);
                    }
                }
                uint64_t id = functorMap.getID(fid, functorArgs.get());
                row[posFromSecond[i].first] = id;
            }
        }
    }
    SingleHeadFinalRuleProcessor::processResults(
            blockid, unique || ignoreDupElimin, NULL);
}

void FunctRuleProcessor::processResults(const int blockid,
        const std::vector<const std::vector<Term_t> *> &vectors1, size_t i1,
        const std::vector<const std::vector<Term_t> *> &vectors2, size_t i2,
        const bool unique) {
    for (int i = 0; i < nCopyFromFirst; i++) {
        if (posFromFirst[i].first != ((uint8_t) - 1)) {
            row[posFromFirst[i].first] = (*vectors1[posFromFirst[i].second])[i1];
        }
    }
    for (int i = 0; i < nCopyFromSecond; i++) {
        if (posFromFirst[i].first != ((uint8_t) - 1)) {
            if (i < nOldCopyFromSecond) {
                row[posFromSecond[i].first] = (*vectors2[posFromSecond[i].second])[i2];
            } else {
                //This is a position added by this object. It represents a functor
                auto &f = functors->at(i - nOldCopyFromSecond);
                auto fid = f.second.fId;
                size_t nargs = f.second.pos.size();
                //go through all the values
                for(size_t j = 0; j < nargs; ++j) {
                    auto pos = f.second.pos[j];
                    //Should I pick it from the left or the right side?
                    if (pos < nCopyFromFirst) {
                        functorArgs[j] = (*vectors1[pos])[i1];
                    } else {
                        functorArgs[j] = (*vectors2[pos - nCopyFromFirst])[i2];
                    }
                }
                uint64_t id = functorMap.getID(fid, functorArgs.get());
                row[posFromSecond[i].first] = id;

            }
        }
    }
    SingleHeadFinalRuleProcessor::processResults(
            blockid, unique || ignoreDupElimin, NULL);
}

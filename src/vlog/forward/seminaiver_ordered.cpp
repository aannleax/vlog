#include "vlog/seminaiver_ordered.h"

#include <iostream>

SemiNaiverOrdered::SemiNaiverOrdered(EDBLayer &layer,
    Program *program, 
    bool opt_intersect,
    bool opt_filtering, 
    bool multithreaded,
    TypeChase chase, 
    int nthreads, 
    bool shuffleRules,
    bool ignoreExistentialRule,
    Program *RMFC_check) : SemiNaiver(layer,
        program, opt_intersect, opt_filtering, multithreaded, chase,
        nthreads, shuffleRules, ignoreExistentialRule, RMFC_check)
{
    std::cout << "Running constructor of SemiNaiverOrdered" << '\n';
}

template<typename GraphType>
void SemiNaiverOrdered::fillOrder(GraphType &graph, unsigned node, 
    std::vector<unsigned> &visited, stack<unsigned> &stack)
{
    visited[node] = 1;
  
    for(unsigned adjacentNode : graph.edges[node])
    {
        if(visited[adjacentNode] == 0)
            fillOrder(graph, adjacentNode, visited, stack);
    }
   
    stack.push(node);
}

template<typename GraphType>
void SemiNaiverOrdered::dfsUntil(GraphType &graph, unsigned node, 
    std::vector<unsigned> &visited, std::vector<unsigned> &currentGroup)
{
    visited[node] = 1;
    currentGroup.push_back(node);

    for(unsigned adjacentNode : graph.edges[node])
    {
        if(visited[adjacentNode] == 0)
            dfsUntil(graph, adjacentNode, visited, currentGroup);
    }
}
template<typename GraphType>
SemiNaiverOrdered::RelianceGroupResult SemiNaiverOrdered::computeRelianceGroups(
    GraphType &graph, GraphType &graphTransposed)
{
    RelianceGroupResult result;
    result.assignments.resize(graph.edges.size(), 0);

    if (graph.edges.size() == 0)
        return result;

    std::stack<unsigned> nodeStack;
    std::vector<unsigned> visited;
    visited.resize(graph.edges.size(), 0);

    for (unsigned node = 0; node < graph.edges.size(); ++node)
    {
        if (visited[node] == 0)
            fillOrder(graph, node, visited, nodeStack);
    }

    std::fill(visited.begin(), visited.end(), 0);

    std::vector<unsigned> currentGroup;
    while (!nodeStack.empty())
    {
        unsigned currentNode = nodeStack.top();
        nodeStack.pop();
  
        if (visited[currentNode] == 0)
        {
            dfsUntil(graphTransposed, currentNode, visited, currentGroup);

            if (currentGroup.size() > 0)
            {
                unsigned currentGroupIndex = result.groups.size();
                result.groups.push_back(currentGroup);

                for (unsigned member : currentGroup)
                {
                    result.assignments[member] = currentGroupIndex;
                }

                currentGroup.clear();
            }
        }
    }

    return result;
}

void SemiNaiverOrdered::prepare(size_t lastExecution, int singleRuleToCheck, const std::vector<Rule> &allRules,
    std::vector<RelianceRuleInfo> &outInfo,
    std::vector<RuleExecutionDetails> &outRuleDetails
)
{
    outInfo.reserve(allRules.size());
    outRuleDetails.reserve(allRules.size());

    for (unsigned ruleIndex = 0; ruleIndex < allRules.size(); ++ruleIndex)
    {
        const Rule &currentRule = allRules[ruleIndex];

        outRuleDetails.emplace_back(currentRule, currentRule.getId());

        outRuleDetails.back().createExecutionPlans(checkCyclicTerms);
        outRuleDetails.back().calculateNVarsInHeadFromEDB();
        outRuleDetails.back().lastExecution = lastExecution;   

        outInfo.emplace_back(ruleIndex, &outRuleDetails.back());
    }

    chaseMgmt = std::shared_ptr<ChaseMgmt>(new ChaseMgmt(outRuleDetails,
        typeChase, checkCyclicTerms,
        singleRuleToCheck,
        predIgnoreBlock));

    // positiveGroups.resize(groupsResult.groups.size());
    // std::vector<RuleExecutionDetails> allRuleDetails;

    // for (unsigned groupIndex = 0; groupIndex < groupsResult.groups.size(); ++groupIndex)
    // {
    //     const std::vector<unsigned> &group = groupsResult.groups[groupIndex];
    //     std::vector<unsigned> groupSorted = group;
    //     auto sortRule = [&allRules] (unsigned index1, unsigned index2) -> bool
    //     {
    //         return allRules[index1].getId() < allRules[index2].getId();
    //     };
        
    //     std::sort(groupSorted.begin(), groupSorted.end(), sortRule);
        
    //     PositiveGroup &newGroup = positiveGroups[groupIndex];

    //     newGroup.rules.reserve(group.size());

    //     std::unordered_set<unsigned> successorSet;
    //     std::unordered_set<unsigned> predecessorSet;
    //     std::unordered_set<unsigned> blockingSet;

    //     for (unsigned ruleIndex : groupSorted)
    //     {
    //         const Rule &currentRule = allRules[ruleIndex];
    //         newGroup.rules.emplace_back(currentRule, currentRule.getId());

    //         newGroup.rules.back().createExecutionPlans(checkCyclicTerms);
    //         newGroup.rules.back().calculateNVarsInHeadFromEDB();
    //         newGroup.rules.back().lastExecution = lastExecution;
        
    //         for (unsigned successor : positiveGraph.edges[ruleIndex])
    //         {
    //             unsigned successorGroup = groupsResult.assignments[successor];
    //             if (successorGroup == groupIndex)
    //                 continue;

    //             successorSet.insert(successorGroup);
    //         }

    //         for (unsigned predecessor : positiveGraphTransposed.edges[ruleIndex])
    //         {
    //             unsigned predecessorGroup = groupsResult.assignments[predecessor];
    //             if (predecessorGroup == groupIndex)
    //                 continue;

    //             predecessorSet.insert(predecessorGroup);
    //         }

    //         for (unsigned blocker : blockingGraphTransposed.edges[ruleIndex])
    //         {
    //             unsigned blockerGroup = groupsResult.assignments[blocker];
    //             if (blockerGroup == groupIndex) //TODO: This is actually possible
    //                 continue;

    //             blockingSet.insert(blockerGroup);
    //         }
    //     }

    //     std::copy(newGroup.rules.begin(), newGroup.rules.end(), std::back_inserter(allRuleDetails));

    //     newGroup.order.resize(newGroup.rules.size());
    //     std::iota(newGroup.order.begin(), newGroup.order.end(), 0);

    //     for (unsigned successor : successorSet)
    //     {
    //         newGroup.successors.push_back(&positiveGroups[0] + successor);
    //     }

    //     for (unsigned predecessor : predecessorSet)
    //     {
    //         newGroup.predecessors.push_back(&positiveGroups[0] + predecessor);
    //     }

    //     for (unsigned blocker : blockingSet)
    //     {
    //         newGroup.blockers.push_back(&positiveGroups[0] + blocker);
    //     }
    // }

    // chaseMgmt = std::shared_ptr<ChaseMgmt>(new ChaseMgmt(allRuleDetails,
    //     typeChase, checkCyclicTerms,
    //     singleRuleToCheck,
    //     predIgnoreBlock));
}

// void SemiNaiverOrdered::orderGroupExistentialLast(PositiveGroup *group)
// {
//     if (group->rules.size() == 1)
//         return;

//     std::vector<unsigned> datalogRules, existentialRules;

//     for (unsigned ruleIndex = 0; ruleIndex < group->rules.size(); ++ruleIndex)
//     {
//         RuleExecutionDetails &currentDetails = group->rules[ruleIndex];

//         if (currentDetails.rule.isExistential())
//         {
//             existentialRules.push_back(ruleIndex);
//         }
//         else
//         {
//             datalogRules.push_back(ruleIndex);
//         }
//     }

//     group->order = datalogRules;
//     group->order.insert(group->order.end(), existentialRules.begin(), existentialRules.end());
// }

// void SemiNaiverOrdered::orderGroupManually(PositiveGroup *group)
// {
//     // const std::vector<unsigned> manualGroup = {0, 9, 8, 3, 5, 1, 7, 6, 4, 2}; // fast
//     // const std::vector<unsigned> manualGroup = {0, 9, 8, 3, 5, 1, 7, 2, 4, 6}; // slow
//     // const std::vector<unsigned> manualGroup = {0, 9, 8, 3, 5, 1, 7, 4, 6, 2}; // slow
//     // const std::vector<unsigned> manualGroup = {0, 8, 3, 5, 1, 7, 9, 6, 2, 4}; // slow
//     // const std::vector<unsigned> manualGroup = {0, 8, 3, 5, 1, 7, 9, 6, 4, 2};
//     // const std::vector<unsigned> manualGroup = {2, 4, 6, 7, 1, 5, 3, 8, 9, 0};
//     const std::vector<unsigned> manualGroup = {9, 6, 4, 2, 0, 7, 3, 5, 1, 7};



//     if (group->order.size() == manualGroup.size())
//         group->order = manualGroup;
// }

// void SemiNaiverOrdered::orderGroupPredicateCount(PositiveGroup *group)
// {
//     if (group->rules.size() == 1)
//         return;

//     const auto sortFunction = [&] (unsigned left, unsigned right) -> bool 
//     {
//         unsigned leftPredicateCount = group->rules[left].rule.getBody().size() + group->rules[left].rule.getHeads().size();
//         unsigned rightPredicateCount = group->rules[right].rule.getBody().size() + group->rules[right].rule.getHeads().size();
        
//         if (leftPredicateCount != rightPredicateCount)
//             return leftPredicateCount < rightPredicateCount;
    
//         unsigned leftArgumentSum = 0, rightArgumentSum = 0;

//         for (const Literal &currentLiteral : group->rules[left].rule.getBody())
//             leftArgumentSum += currentLiteral.getPredicate().getCardinality();
        
//         for (const Literal &currentLiteral : group->rules[left].rule.getHeads())
//             leftArgumentSum += currentLiteral.getPredicate().getCardinality();
        
//         for (const Literal &currentLiteral : group->rules[right].rule.getBody())
//             rightArgumentSum += currentLiteral.getPredicate().getCardinality();
        
//         for (const Literal &currentLiteral : group->rules[right].rule.getHeads())
//             rightArgumentSum += currentLiteral.getPredicate().getCardinality();
        
//         return leftArgumentSum < rightArgumentSum;
//     };

//     std::sort(group->order.begin(), group->order.end(), sortFunction);

//     for (unsigned index : group->order)
//     {
//         std::cout << group->rules[index].rule.getId() << "(" << index << ") ";
//     }
//     std::cout << '\n';
// }


// //TODO: Does not support blocking
// bool SemiNaiverOrdered::executeGroupAverageRuntime(
//     std::vector<RuleExecutionDetails> &ruleset,
//     std::vector<StatIteration> &costRules,
//     bool blocked, unsigned long *timeout)
// {
//     bool result = false;

//     size_t currentRule = 0;
//     uint32_t rulesWithoutDerivation = 0; 

//     size_t nRulesOnePass = 0;
//     size_t lastIteration = 0;

//     double falseExecutionTimeSum = 0.0;

//     std::chrono::system_clock::time_point executionStart = std::chrono::system_clock::now();
//     do 
//     {
//         std::chrono::system_clock::time_point iterationStart = std::chrono::system_clock::now();
        
//         ruleset[currentRule].cardinalitySum = 0.0;
//         for (const Literal &currentLiteral : ruleset[currentRule].rule.getBody())
//         {
//             if (currentLiteral.getPredicate().getType() == EDB)
//             {
//                 ruleset[currentRule].cardinalitySum += (double)layer.estimateCardinality(currentLiteral);
//             }
//             else
//             {
//                 ruleset[currentRule].cardinalitySum += (double)estimateCardinality(currentLiteral, 0, iteration);
//             }
//         }

//         bool response = executeRule(ruleset[currentRule], iteration, 0, NULL);

//         result |= response;

//         if (timeout != NULL && *timeout != 0) 
//         {
//             std::chrono::duration<double> runDuration = std::chrono::system_clock::now() - startTime;
//             if (runDuration.count() > *timeout) {
//                 *timeout = 0;   // To indicate materialization was stopped because of timeout.
//                 return result;
//             }
//         }

//         std::chrono::duration<double> iterationDuration = std::chrono::system_clock::now() - iterationStart;
//         StatIteration stat;
//         stat.iteration = iteration;
//         stat.rule = &ruleset[currentRule].rule;
//         stat.time = iterationDuration.count() * 1000;
//         stat.derived = response;
//         costRules.push_back(stat);

//         ruleset[currentRule].executionTime += (double)iterationDuration.count();
//         ruleset[currentRule].lastExecution = iteration;
//         iteration++;

//         if (!response)
//         {
//             // std::cout << "False execution: " << (double)iterationDuration.count() << '\n';
//             falseExecutionTimeSum += (double)iterationDuration.count();
//         }

//         if (checkCyclicTerms) {
//             foundCyclicTerms = chaseMgmt->checkCyclicTerms(currentRule);
//             if (foundCyclicTerms) {
//                 LOG(DEBUGL) << "Found a cyclic term";
//                 return result;
//             }
//         }

//         if (response)
//         {
//             rulesWithoutDerivation = 0;
//             nRulesOnePass++;
//         }
//         else
//         {
//             rulesWithoutDerivation++;
//         }

//         currentRule = 0;
//         double ruleEffectiveness = 0.0; // std::numeric_limits<double>::infinity();
//         bool noNextRule = true;
//         for (unsigned ruleIndex = 0; ruleIndex < ruleset.size(); ++ruleIndex)
//         {
//             RuleExecutionDetails &currentDetails = ruleset[ruleIndex];
//             Rule rule = currentDetails.rule;

//             if (!bodyChangedSince(rule, currentDetails.lastExecution))
//                 continue;

//             if (currentDetails.executionTime == 0)
//             {
//                 currentRule = ruleIndex;
//                 noNextRule = false;
//                 break;
//             }

//             double currentEffectiveness = currentDetails.cardinalitySum / currentDetails.executionTime;

//             //NOTE: We execute effective rules first
//             if (currentEffectiveness > ruleEffectiveness)
//             {
//                 currentRule = ruleIndex;
//                 ruleEffectiveness = currentEffectiveness;
//             }

//             noNextRule = false;
//         }

//         if (noNextRule)
//             break;
//     } while (rulesWithoutDerivation != ruleset.size());

//     for (unsigned ruleIndex = 0; ruleIndex < ruleset.size(); ++ruleIndex)
//     {
//         RuleExecutionDetails &currentDetails = ruleset[ruleIndex];
    
//         double currentEffectiveness = currentDetails.cardinalitySum / currentDetails.executionTime;

//         std::cout << currentEffectiveness << ", " << currentDetails.executionTime << ": " << currentDetails.rule.tostring(program, &layer) << '\n';
//     }

//     std::cout << "False Execution time: " << falseExecutionTimeSum << '\n';

//     return result;
// }

// bool SemiNaiverOrdered::executeGroupBottomUp(
//     std::vector<RuleExecutionDetails> &ruleset,
//     std::vector<unsigned> &rulesetOrder,
//     std::vector<StatIteration> &costRules,
//     bool blocked, unsigned long *timeout
// )
// {
//     bool result = false;

//     std::chrono::system_clock::time_point executionStart = std::chrono::system_clock::now();
//     for (unsigned currentRuleIndex = 0; currentRuleIndex < ruleset.size(); ++currentRuleIndex)
//     {
//         unsigned currentRule = rulesetOrder[currentRuleIndex];
//         RuleExecutionDetails &currentRuleDetail = ruleset[currentRule];

//         std::chrono::system_clock::time_point iterationStart = std::chrono::system_clock::now();
        
//         bool response = executeRule(currentRuleDetail, iteration, 0, NULL);
//         result |= response;

//         if (timeout != NULL && *timeout != 0) 
//         {
//             std::chrono::duration<double> runDuration = std::chrono::system_clock::now() - startTime;
//             if (runDuration.count() > *timeout) {
//                 *timeout = 0;   // To indicate materialization was stopped because of timeout.
//                 return result;
//             }
//         }

//         std::chrono::duration<double> iterationDuration = std::chrono::system_clock::now() - iterationStart;
//         StatIteration stat;
//         stat.iteration = iteration;
//         stat.rule = &ruleset[currentRule].rule;
//         stat.time = iterationDuration.count() * 1000;
//         stat.derived = response;
//         costRules.push_back(stat);

//         ruleset[currentRule].lastExecution = iteration;
//         iteration++;

//         if (checkCyclicTerms) {
//             foundCyclicTerms = chaseMgmt->checkCyclicTerms(currentRule);
//             if (foundCyclicTerms) {
//                 LOG(DEBUGL) << "Found a cyclic term";
//                 return result;
//             }
//         }

//         if (response)
//             currentRuleIndex = (unsigned)-1;
//     }

//     return result;
// }

// bool SemiNaiverOrdered::executeGroupInOrder(
//     std::vector<RuleExecutionDetails> &ruleset,
//     std::vector<unsigned> &rulesetOrder,
//     std::vector<StatIteration> &costRules,
//     bool blocked, unsigned long *timeout
// )
// {
//     bool result = false;

//     size_t currentRuleIndex = 0;
//     uint32_t rulesWithoutDerivation = 0; 

//     size_t nRulesOnePass = 0;
//     size_t lastIteration = 0;

//     std::chrono::system_clock::time_point executionStart = std::chrono::system_clock::now();
//     do 
//     {
//         size_t currentRule = rulesetOrder[currentRuleIndex];
//         std::chrono::system_clock::time_point iterationStart = std::chrono::system_clock::now();
//         bool response = executeRule(ruleset[currentRule], iteration, 0, NULL);

//         result |= response;

//         if (timeout != NULL && *timeout != 0) 
//         {
//             std::chrono::duration<double> runDuration = std::chrono::system_clock::now() - startTime;
//             if (runDuration.count() > *timeout) {
//                 *timeout = 0;   // To indicate materialization was stopped because of timeout.
//                 return result;
//             }
//         }

//         std::chrono::duration<double> iterationDuration = std::chrono::system_clock::now() - iterationStart;
//         StatIteration stat;
//         stat.iteration = iteration;
//         stat.rule = &ruleset[currentRule].rule;
//         stat.time = iterationDuration.count() * 1000;
//         stat.derived = response;
//         costRules.push_back(stat);

//         ruleset[currentRule].lastExecution = iteration;
//         iteration++;

//         if (checkCyclicTerms) {
//             foundCyclicTerms = chaseMgmt->checkCyclicTerms(currentRule);
//             if (foundCyclicTerms) {
//                 LOG(DEBUGL) << "Found a cyclic term";
//                 return result;
//             }
//         }

//         if (response)
//         {
//             rulesWithoutDerivation = 0;
//             nRulesOnePass++;
//         }
//         else
//         {
//             rulesWithoutDerivation++;
//         }

//         currentRuleIndex = (currentRuleIndex + 1) % ruleset.size();

//         if (currentRuleIndex == 0) 
//         {
//             if (!blocked)
//                 return result;
//         }
//     } while (rulesWithoutDerivation != ruleset.size());

//     return result;
// }

SemiNaiverOrdered::PositiveGroup *SemiNaiverOrdered::executeRestrainedGroup(
    RestrainedGroup &group, 
    std::vector<StatIteration> &costRules, unsigned long *timeout
)
{
    struct PriorityGroup
    {
        std::vector<RelianceRuleInfo*> &members;
        size_t currentRuleIndex = 0;
        size_t numWithoutDerivations = 0;
        bool containsTriggeredRule = false;

        PriorityGroup(std::vector<RelianceRuleInfo *> &infos)
            : members(infos)
        {
            for (const RelianceRuleInfo* member: members)
            {
                if (member->triggered)
                {
                    containsTriggeredRule= true;
                    break;
                }
            } 
        }
    } restrainedRules(group.restrainedMembers), unrestrainedRules(group.unrestrainedMembers);

    std::chrono::system_clock::time_point executionStart = std::chrono::system_clock::now();
    while (restrainedRules.containsTriggeredRule || unrestrainedRules.containsTriggeredRule)
    {
        PriorityGroup &currentGroup = (unrestrainedRules.containsTriggeredRule) ? unrestrainedRules : restrainedRules;
        RelianceRuleInfo &currentRuleInfo = *currentGroup.members[currentGroup.currentRuleIndex];

        currentGroup.currentRuleIndex = (currentGroup.currentRuleIndex + 1) % currentGroup.members.size();

        if (!currentRuleInfo.triggered)
        {
            ++currentGroup.numWithoutDerivations;

            if (currentGroup.numWithoutDerivations == currentGroup.members.size())
            {
                currentGroup.containsTriggeredRule = false;
            }

            continue;
        }

        std::chrono::system_clock::time_point iterationStart = std::chrono::system_clock::now();
        bool response = executeRule(*currentRuleInfo.ruleDetails, iteration, 0, NULL);

        if (timeout != NULL && *timeout != 0) 
        {
            std::chrono::duration<double> runDuration = std::chrono::system_clock::now() - startTime;
            if (runDuration.count() > *timeout) {
                *timeout = 0;   // To indicate materialization was stopped because of timeout.
                return nullptr;
            }
        }
        std::chrono::duration<double> iterationDuration = std::chrono::system_clock::now() - iterationStart;
        StatIteration stat;
        stat.iteration = iteration;
        stat.rule = &currentRuleInfo.ruleDetails->rule;
        stat.time = iterationDuration.count() * 1000;
        stat.derived = response;
        costRules.push_back(stat);

        currentRuleInfo.ruleDetails->executionTime += (double)iterationDuration.count();
        currentRuleInfo.ruleDetails->lastExecution = iteration;
        iteration++;

        currentRuleInfo.setTriggered(false);

        if (response)
        {
            for (RelianceRuleInfo *successor : currentRuleInfo.positiveSuccessors)
            {
                successor->setTriggered(true);

                if (successor->positiveGroup->id == currentRuleInfo.positiveGroup->id)
                {
                    if (successor->numRestrains == 0)
                    {
                        unrestrainedRules.containsTriggeredRule = true;
                        unrestrainedRules.numWithoutDerivations = 0;
                        unrestrainedRules.currentRuleIndex = 0;
                    }
                    else
                    {
                        restrainedRules.containsTriggeredRule = true;
                        restrainedRules.numWithoutDerivations = 0;
                        restrainedRules.currentRuleIndex = 0;
                    }
                }
            }
        }

        if (!currentRuleInfo.positiveGroup->isActive())
        {
            return currentRuleInfo.positiveGroup;
        }
    }

    return nullptr;
}

void SemiNaiverOrdered::sortRuleVectorById(std::vector<RelianceRuleInfo *> &infos)
{
    auto sortRule = [] (RelianceRuleInfo *rule1, RelianceRuleInfo *rule2) -> bool
    {
        return rule1->ruleDetails->rule.getId() < rule2->ruleDetails->rule.getId();
    };
    
    std::sort(infos.begin(), infos.end(), sortRule);
}

std::vector<SemiNaiverOrdered::PositiveGroup> SemiNaiverOrdered::computePositiveGroups(
    std::vector<RelianceRuleInfo> &allRules,
    SimpleGraph &positiveGraph,
    RelianceGroupResult &groupsResult
)
{
    std::vector<PositiveGroup> result;

    result.resize(groupsResult.groups.size());

    for (unsigned groupIndex = 0; groupIndex < groupsResult.groups.size(); ++groupIndex)
    {
        const std::vector<unsigned> &group = groupsResult.groups[groupIndex];
        
        PositiveGroup &newGroup = result[groupIndex];
        newGroup.members.reserve(group.size());
        newGroup.id = groupIndex;

        std::unordered_set<unsigned> successorSet;
        
        for (unsigned ruleIndex : group)
        {
            newGroup.members.push_back(&allRules[0] + ruleIndex);
        
            for (unsigned successor : positiveGraph.edges[ruleIndex])
            {
                unsigned successorGroup = groupsResult.assignments[successor];
                if (successorGroup == groupIndex)
                    continue;

                successorSet.insert(successorGroup);
            }
        }

        sortRuleVectorById(newGroup.members);

        for (unsigned successor : successorSet)
        {
            PositiveGroup *successorGroup = &result[0] + successor;
            newGroup.successors.push_back(successorGroup);
            ++successorGroup->numActivePredecessors;
        }
    }

    return result;
}

void SemiNaiverOrdered::updateGraph(UnorderedGraph &graph, UnorderedGraph &graphTransposed, PositiveGroup *group)
{
    if (group->isActive())
        return;

    group->setInactive();

    for (RelianceRuleInfo *member : group->members)
    {
        graph.removeNode(member->id);
        graphTransposed.removeNode(member->id);
    }

    for (PositiveGroup *successorGroup : group->successors)
    {
        updateGraph(graph, graphTransposed, successorGroup);
    }
}

bool SemiNaiverOrdered::executeGroup(
    std::vector<RuleExecutionDetails> &ruleset,
    std::vector<StatIteration> &costRules,
    bool blocked, unsigned long *timeout
)
{
    bool result = false;


//     size_t currentRule = 0;
//     uint32_t rulesWithoutDerivation = 0; 

//     size_t nRulesOnePass = 0;
//     size_t lastIteration = 0;

//     double falseExecutionTimeSum = 0.0;

//     std::chrono::system_clock::time_point executionStart = std::chrono::system_clock::now();
//     do 
//     {
//         std::chrono::system_clock::time_point iterationStart = std::chrono::system_clock::now();
//         bool response = executeRule(ruleset[currentRule], iteration, 0, NULL);

//         result |= response;

//         if (timeout != NULL && *timeout != 0) 
//         {
//             std::chrono::duration<double> runDuration = std::chrono::system_clock::now() - startTime;
//             if (runDuration.count() > *timeout) {
//                 *timeout = 0;   // To indicate materialization was stopped because of timeout.
//                 return result;
//             }
//         }

//         std::chrono::duration<double> iterationDuration = std::chrono::system_clock::now() - iterationStart;
//         StatIteration stat;
//         stat.iteration = iteration;
//         stat.rule = &ruleset[currentRule].rule;
//         stat.time = iterationDuration.count() * 1000;
//         stat.derived = response;
//         costRules.push_back(stat);

//         ruleset[currentRule].executionTime += (double)iterationDuration.count();
//         ruleset[currentRule].lastExecution = iteration;
//         iteration++;

//         if (!response)
//             falseExecutionTimeSum += (double)iterationDuration.count();;

//         if (checkCyclicTerms) {
//             foundCyclicTerms = chaseMgmt->checkCyclicTerms(currentRule);
//             if (foundCyclicTerms) {
//                 LOG(DEBUGL) << "Found a cyclic term";
//                 return result;
//             }
//         }

//         if (response)
//         {
//             rulesWithoutDerivation = 0;
//             nRulesOnePass++;
//         }
//         else
//         {
//             rulesWithoutDerivation++;
//         }

//         currentRule = (currentRule + 1) % ruleset.size();

//         if (currentRule == 0) 
//         {
//             if (!blocked)
//                 return result;
// #ifdef DEBUG
//             std::chrono::duration<double> sec = std::chrono::system_clock::now() - executionStart;
//             LOG(DEBUGL) << "--Time round " << sec.count() * 1000 << " " << iteration;
//             executionStart = std::chrono::system_clock::now();
//             //CODE FOR Statistics
//             LOG(INFOL) << "Finish pass over the rules. Step=" << iteration << ". IDB RulesWithDerivation=" <<
//                 nRulesOnePass << " out of " << ruleset.size() << " Derivations so far " << countAllIDBs();
//             printCountAllIDBs("After step " + to_string(iteration) + ": ");
//             nRulesOnePass = 0;

//             //Get the top 10 rules in the last iteration
//             std::sort(costRules.begin(), costRules.end());
//             std::string out = "";
//             int n = 0;
//             for (const auto &exec : costRules) {
//                 if (exec.iteration >= lastIteration) {
//                     if (n < 10 || exec.derived) {
//                         out += "Iteration " + to_string(exec.iteration) + " runtime " + to_string(exec.time);
//                         out += " " + exec.rule->tostring(program, &layer) + " response " + to_string(exec.derived);
//                         out += "\n";
//                     }
//                     n++;
//                 }
//             }
//             LOG(DEBUGL) << "Rules with the highest cost\n\n" << out;
//             lastIteration = iteration;
//             //END CODE STATISTICS
// #endif
//         }
//     } while (rulesWithoutDerivation != ruleset.size());

//     // for (unsigned ruleIndex = 0; ruleIndex < ruleset.size(); ++ruleIndex)
//     // {
//     //     RuleExecutionDetails &currentDetails = ruleset[ruleIndex];
    
//     //     double currentEffectiveness = currentDetails.cardinalitySum / currentDetails.executionTime;

//     //     std::cout << currentEffectiveness << ", " << currentDetails.executionTime << ": " << currentDetails.rule.tostring(program, &layer) << '\n';
//     // }

//     // std::cout << "False Execution time: " << falseExecutionTimeSum << '\n';

    return result;
}

// void SemiNaiverOrdered::setActive(PositiveGroup *currentGroup)
// {
//     currentGroup->active = true;

//     for (PositiveGroup *successorGroup : currentGroup->successors)
//     {
//         setActive(successorGroup);
//     }
// }

// std::pair<SimpleGraph, SimpleGraph> DEBUGEveryIDBInOneGroup(std::vector<Rule> &rules)
// {
//     SimpleGraph result(rules.size());
//     SimpleGraph resultTransposed(rules.size());

//     int firstIDB = -1;
//     int firstEDB = -1;
//     int previousIDB = -1;
//     int previousEDB = -1;

//     for (unsigned ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex)
//     {
//         const Rule &currentRule = rules[ruleIndex];

//         bool isIDB = currentRule.getNIDBPredicates() > 0;

//         if (isIDB && previousIDB == -1)
//         {
//             previousIDB = ruleIndex;
//             firstIDB = ruleIndex;

//             continue;
//         }
//         else if (!isIDB && previousEDB == -1)
//         {
//             previousEDB = ruleIndex;
//             firstEDB = ruleIndex;

//             continue;
//         }

//         if (isIDB)
//         {
//             result.addEdge(previousIDB, ruleIndex);
//             resultTransposed.addEdge(ruleIndex, previousIDB);
//             previousIDB = ruleIndex;
//         }
//         else
//         {
//             result.addEdge(previousEDB, ruleIndex);
//             resultTransposed.addEdge(ruleIndex, previousEDB);
//             previousEDB = ruleIndex;
//         }
//     }

//     result.addEdge(previousIDB, firstIDB);
//     resultTransposed.addEdge(firstIDB, previousIDB);
//     result.addEdge(previousEDB, firstEDB);
//     resultTransposed.addEdge(firstEDB, previousEDB);

//     result.addEdge(firstEDB, firstIDB);
//     resultTransposed.addEdge(firstIDB, firstEDB);

//     return std::make_pair(result, resultTransposed);
// }

// std::pair<SimpleGraph, SimpleGraph> DEBUGblockingGraphOfLUBM(unsigned ruleSize)
// {
//     SimpleGraph result(ruleSize), resultTransposed(ruleSize);

//     std::vector<unsigned> neighbors42 =
//         {4, 10};

//     std::vector<unsigned> neighbors49 =
//         {10};

//     std::vector<unsigned> neighbors55 =
//         {10};

//     std::vector<unsigned> neighbors62 = 
//         {29, 33, 34, 46, 54, 72, 73, 82, 86, 95, 104, 114, 115, 132};

//     std::vector<unsigned> neighbors67 =
//         {21, 8};

//     std::vector<unsigned> neighbors103 = 
//         {18, 29, 72, 106};
    
//     std::vector<unsigned> neighbors113 = 
//         {3, 21, 65, 76, 118, 122};

//     std::vector<unsigned> neighbors121 = 
//         {3, 24, 65, 76, 118, 122};

//     auto addAllEdges = [&] (unsigned node, const std::vector<unsigned> &neighbors) -> void {
//         for (unsigned neighbor : neighbors)
//         {
//             result.addEdge(neighbor, node);
//             resultTransposed.addEdge(node, neighbor);
//         }
//     };

//     addAllEdges(42, neighbors42);
//     addAllEdges(49, neighbors49);
//     addAllEdges(55, neighbors55);
//     addAllEdges(62, neighbors62);
//     addAllEdges(67, neighbors67);
//     addAllEdges(103, neighbors103);
//     addAllEdges(113, neighbors113);
//     addAllEdges(121, neighbors121);
    
//     return std::make_pair(result, resultTransposed);
// }

#define RUNMAT 1

void createPiece(const std::vector<Literal> &heads, unsigned currentLiteralIndex, std::vector<bool> &literalsInPieces, const std::vector<uint32_t> &existentialVariables, std::vector<Literal> &result)
{
    const Literal &currentLiteral = heads[currentLiteralIndex];
    literalsInPieces[currentLiteralIndex] = true;

    for (unsigned termIndex = 0; termIndex < currentLiteral.getTupleSize(); ++termIndex)
    {
        VTerm currentTerm = currentLiteral.getTermAtPos(termIndex);

        if (std::find(existentialVariables.begin(), existentialVariables.end(), currentTerm.getId()) == existentialVariables.end())
            continue;

        for (unsigned searchIndex = currentLiteralIndex + 1; searchIndex < heads.size(); ++searchIndex)
        {
            if (literalsInPieces[searchIndex])
                continue;

            const Literal &currentSearchedLiteral = heads[searchIndex];
            for (unsigned searchTermIndex = 0; searchTermIndex < currentSearchedLiteral.getTupleSize(); ++searchTermIndex)
            {
                VTerm currentSearchedTerm = currentSearchedLiteral.getTermAtPos(searchTermIndex);
            
                if (currentTerm.getId() == currentSearchedTerm.getId())
                {
                    createPiece(heads, searchIndex, literalsInPieces, existentialVariables, result);
                }
            }
        }
    }
    
    result.push_back(currentLiteral);
}

void splitIntoPieces(const Rule &rule, std::vector<Rule> &outRules)
{
    uint32_t ruleId = outRules.size();
    std::vector<Literal> body = rule.getBody();
    std::vector<Literal> heads = rule.getHeads();

    std::vector<Var_t> existentialVariables = rule.getExistentialVariables();

    std::vector<bool> literalsInPieces;
    literalsInPieces.resize(heads.size(), false);

    for (unsigned literalIndex = 0; literalIndex < heads.size(); ++literalIndex)
    {
        const Literal &currentLiteral = heads[literalIndex];
        
        if (literalsInPieces[literalIndex])
            continue;

        std::vector<Literal> newPiece;
        createPiece(heads, literalIndex, literalsInPieces, existentialVariables, newPiece);

        if (newPiece.size() > 0)
        {
            outRules.emplace_back(ruleId, newPiece, body);
            ++ruleId;
        }   
    }
}

std::pair<UnorderedGraph, UnorderedGraph> SemiNaiverOrdered::combineGraphs(
    const SimpleGraph &positiveGraph, const SimpleGraph &restraintGraph)
{
    UnorderedGraph unionGraph, unionGraphTransposed;

    auto copyGraph = [] (const SimpleGraph &simple, UnorderedGraph &outUnion, UnorderedGraph &outUnionTransposed) -> void
    {
        for (unsigned fromNode = 0; fromNode < simple.edges.size(); ++fromNode)
        {
            for (unsigned toNode : simple.edges[fromNode])
            {
                outUnion.addEdge(fromNode, toNode);
                outUnionTransposed.addEdge(toNode, fromNode);
            }
        }
    };

    copyGraph(positiveGraph, unionGraph, unionGraphTransposed);
    copyGraph(restraintGraph, unionGraph, unionGraphTransposed);
    
    return std::make_pair(unionGraph, unionGraphTransposed);
}

void SemiNaiverOrdered::run(size_t lastExecution,
    size_t iteration,
    unsigned long *timeout,
    bool checkCyclicTerm,
    int singleRule,
    PredId_t predIgnoreBlock)
{
    this->checkCyclicTerms = checkCyclicTerms;
    this->foundCyclicTerms = false;
    this->predIgnoreBlock = predIgnoreBlock;
    this->running = true;
    this->startTime = std::chrono::system_clock::now();
    listDerivations.clear();

    this->iteration = iteration;

    //std::vector<Rule> &allOriginalRules = program->getAllRules();
    std::vector<Rule> allRules = program->getAllRules();
    /*for (const Rule &currentRule : allOriginalRules)
    {
        splitIntoPieces(currentRule, allRules);
    }*/

    // for (const Rule &currentRule : allRules)
    // {
    //     std::cout << currentRule.tostring(program, &layer) << std::endl;
    // }

    std::cout << "#Rules: " << allRules.size() << '\n';

    std::chrono::system_clock::time_point relianceStart = std::chrono::system_clock::now();
    std::cout << "Computing positive reliances..." << '\n';
    std::pair<SimpleGraph, SimpleGraph> positiveGraphs = computePositiveReliances(allRules);
    // std::pair<SimpleGraph, SimpleGraph> SimpleGraphs = DEBUGEveryIDBInOneGroup(allRules);

    /*std::cout << "Positive reliances: " << '\n';
    for (unsigned from = 0; from < SimpleGraphs.first.edges.size(); ++from)
    {
        for (unsigned to :  SimpleGraphs.first.edges[from])
        {
            std::cout << "positive reliance: " << from << " -> " << to << '\n';
        }
    }*/

    // unsigned fakeReliances = DEBUGcountFakePositiveReliances(allRules, SimpleGraphs.first);
    // std::cout << "There are " << fakeReliances << " fake reliances." << '\n';

    positiveGraphs.first.saveCSV("positive_V2.csv");

    std::cout << "Computing positive reliance groups..." << '\n';
    RelianceGroupResult positiveGroupsResult = computeRelianceGroups(positiveGraphs.first, positiveGraphs.second);    
    std::cout << "Reliance computation took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - relianceStart).count() << '\n';

    /*for (vector<unsigned> &group : relianceGroups.groups)
    {
        if (group.size() < 2)
            continue;

        for (unsigned member : group)
        {
            std::cout << allRules[member].tostring(program, &layer) << '\n';
        }
        std::cout << "------------------------" << '\n';
    }

    std::cout << "Found " << relianceGroups.groups.size() << " groups." << '\n';*/

    std::vector<StatIteration> costRules;

    //std::pair<SimpleGraph, SimpleGraph> blockingGraphs = std::make_pair(SimpleGraph(allRules.size()), SimpleGraph(allRules.size()));
    std::cout << "Computing restraint reliances..." << '\n';
    relianceStart = std::chrono::system_clock::now();
    std::pair<SimpleGraph, SimpleGraph> restrainingGraphs = computeRestrainReliances(allRules);
    // std::pair<SimpleGraph, SimpleGraph> blockingGraphs = DEBUGblockingGraphOfLUBM(allRules.size());
    std::cout << "Restraint computation took " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - relianceStart).count() << '\n';

    restrainingGraphs.first.saveCSV("blocking_V2.csv");

    // for (unsigned blockedIndex = 0; blockedIndex < blockingGraphs.second.edges.size(); ++blockedIndex)
    // {
    //     const std::vector<unsigned> blockingNodes = blockingGraphs.second.edges[blockedIndex];

    //     if (blockingNodes.size() > 0)
    //         std::cout << allRules[blockedIndex].tostring(program, &layer) << '\n';
        
    //     for (unsigned blockingNode : blockingNodes)
    //     {
    //         std::cout << '\t' << allRules[blockingNode].tostring(program, &layer) << '\n';
    //     }
    // }
    // std::cout << '\n';

#if RUNMAT
    std::vector<RuleExecutionDetails> allRuleDetails;
    std::vector<RelianceRuleInfo> allRuleInfos;
    prepare(lastExecution, singleRule, allRules, allRuleInfos, allRuleDetails);

    std::vector<PositiveGroup> positiveGroups = computePositiveGroups(allRuleInfos, positiveGraphs.first, positiveGroupsResult);

    for (RelianceRuleInfo &currentInfo : allRuleInfos)
    {
        std::vector<RelianceRuleInfo *> positiveSuccessors, restraintSuccessors;
        for (unsigned successorIndex : positiveGraphs.first.edges[currentInfo.id])
        {
            positiveSuccessors.push_back(&allRuleInfos[0] + successorIndex);
        }

        for (unsigned successorIndex : restrainingGraphs.first.edges[currentInfo.id])
        {
            restraintSuccessors.push_back(&allRuleInfos[0] + successorIndex);
        }

        PositiveGroup *currentPositiveGroup = &positiveGroups[0] + positiveGroupsResult.assignments[currentInfo.id];
    
        currentInfo.initialize(currentPositiveGroup, positiveSuccessors, restraintSuccessors);
    }

    std::pair<UnorderedGraph, UnorderedGraph> unionGraphs = combineGraphs(positiveGraphs.first, restrainingGraphs.first);

    for (RelianceRuleInfo &currentInfo : allRuleInfos)
    {
        if (currentInfo.ruleDetails->nIDBs == 0)
        {
            currentInfo.setTriggered(true);
        }
    }

    for (unsigned dummyNode = 0; dummyNode < allRuleInfos.size(); ++dummyNode)
    {
        auto x = unionGraphs.first.edges[dummyNode];
        auto y = unionGraphs.second.edges[dummyNode];
    }

    for (PositiveGroup &currentGroup : positiveGroups)
    {
        updateGraph(unionGraphs.first, unionGraphs.second, &currentGroup);
    }

    RelianceGroupResult combinedGroupsResult = computeRelianceGroups(unionGraphs.first, unionGraphs.second);    
    std::cout << "Number of groups: " << combinedGroupsResult.groups.size() << std::endl;

    // std::vector<PositiveGroup> positiveGroups;
    // prepare(lastExecution, singleRule, allRules, SimpleGraphs.first, SimpleGraphs.second, blockingGraphs.second, relianceGroups, positiveGroups);

    // std::deque<PositiveGroup *> positiveStack;

    // for (PositiveGroup &currentGroup : positiveGroups)
    // {
    //     for (const RuleExecutionDetails &details : currentGroup.rules)
    //     {
    //         if (details.nIDBs == 0)
    //         {
    //             positiveStack.push_front(&currentGroup);
    //             currentGroup.inQueue = true;
    //             currentGroup.triggered = true;

    //             setActive(&currentGroup);
    //         }
    //     }
    // }

    // PositiveGroup *firstBlockedGroup = nullptr;
    // while (!positiveStack.empty())
    // {
    //     PositiveGroup *currentGroup = positiveStack.front();
    //     positiveStack.pop_front();
    //     currentGroup->inQueue = false;

    //     bool hasActivePredecessors = false;
    //     for (PositiveGroup *predecessorGroup : currentGroup->predecessors)
    //     {
    //         if (predecessorGroup->active)
    //         {
    //             hasActivePredecessors = true;
    //             break;
    //         }
    //     }

    //     if (!hasActivePredecessors)
    //     {
    //         bool newDerivations = false;
    //         bool isBlocked = false;
    //         bool hasBeenExecuted = false;
            
    //         if (currentGroup->triggered)
    //         {
    //             // orderGroupManually(currentGroup);
    //             // orderGroupPredicateCount(currentGroup);

    //             for (PositiveGroup *blockingGroup : currentGroup->blockers)
    //             {
    //                 if (blockingGroup->active)
    //                 {
    //                     isBlocked = true;
    //                     break;
    //                 }
    //             }

    //             //if (currentGroup == firstBlockedGroup)
    //                 //std::cout << "Queue has no unblocked triggered members" << '\n';

    //             if (!isBlocked || currentGroup == firstBlockedGroup)
    //             {
    //                 if (isBlocked)
    //                     std::cout << "blocked" << '\n';

    //                 if (!isBlocked)
    //                     std::cout << "not blocked" << '\n';

    //                 newDerivations = executeGroup(currentGroup->rules, costRules, !isBlocked, timeout);
    //                 // newDerivations = executeGroupInOrder(currentGroup->rules, currentGroup->order, costRules, !isBlocked, timeout);
    //                 // newDerivations = executeGroupBottomUp(currentGroup->rules, currentGroup->order, costRules, !isBlocked, timeout);
    //                 // newDerivations = executeGroupAverageRuntime(currentGroup->rules, costRules, !isBlocked, timeout);
                
    //                 hasBeenExecuted = true;
    //             }

    //             if (isBlocked && firstBlockedGroup == nullptr)
    //             {
    //                 firstBlockedGroup = currentGroup;
    //             }
    //             else if (currentGroup == firstBlockedGroup)
    //             {
    //                 firstBlockedGroup = nullptr;
    //             }
    //         }

    //         currentGroup->active = false;

    //         if (isBlocked && (!hasBeenExecuted || newDerivations))
    //         {
    //             positiveStack.push_back(currentGroup);
    //             currentGroup->inQueue = true;
    //             currentGroup->active = true;
    //         }

    //         if (newDerivations || !currentGroup->active)
    //         {
    //             for (PositiveGroup *successorGroup : currentGroup->successors)
    //             {
    //                 if (newDerivations)
    //                 {
    //                     successorGroup->triggered = true;
    //                 }

    //                 if (!successorGroup->inQueue)
    //                 {
    //                     positiveStack.push_front(successorGroup);   
    //                     successorGroup->inQueue = true;
    //                 }
    //             }
    //         }
    //     }
    // }
#endif

    std::cout << "Iterations: " << this->iteration << ", ExecuteRule Calls: " << executeRuleCount << ", true: " << executeRuleTrueCount << '\n';

    this->running = false;
}
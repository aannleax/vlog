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
    std::cout << "Running constructor of SemiNaiverOrdered" << std::endl;
}

void SemiNaiverOrdered::fillOrder(const RelianceGraph &graph, unsigned node, 
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

void SemiNaiverOrdered::dfsUntil(const RelianceGraph &graph, unsigned node, 
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

SemiNaiverOrdered::RelianceGroupResult SemiNaiverOrdered::computeRelianceGroups(
    const RelianceGraph &graph, const RelianceGraph &graphTransposed)
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
    const RelianceGraph &positiveGraph, const RelianceGraph &positiveGraphTransposed,
    const RelianceGroupResult &groupsResult, 
    std::vector<PositiveGroup> &positiveGroups)
{
    positiveGroups.resize(groupsResult.groups.size());
    std::vector<RuleExecutionDetails> allRuleDetails;

    for (unsigned groupIndex = 0; groupIndex < groupsResult.groups.size(); ++groupIndex)
    {
        const std::vector<unsigned> &group = groupsResult.groups[groupIndex];
        PositiveGroup &newGroup = positiveGroups[groupIndex];

        newGroup.rules.reserve(group.size());

        std::unordered_set<unsigned> successorSet;
        std::unordered_set<unsigned> predecessorSet;

        for (unsigned ruleIndex : group)
        {
            const Rule &currentRule = allRules[ruleIndex];
            newGroup.rules.emplace_back(currentRule, currentRule.getId()); //TODO: Check this Id field

            newGroup.rules.back().createExecutionPlans(checkCyclicTerms);
            newGroup.rules.back().calculateNVarsInHeadFromEDB();
            newGroup.rules.back().lastExecution = lastExecution;
        
            for (unsigned successor : positiveGraph.edges[ruleIndex])
            {
                unsigned successorGroup = groupsResult.assignments[successor];
                if (successorGroup == groupIndex)
                    continue;

                successorSet.insert(successorGroup);
            }

            for (unsigned predecessor : positiveGraphTransposed.edges[ruleIndex])
            {
                unsigned predecessorGroup = groupsResult.assignments[predecessor];
                if (predecessorGroup == groupIndex)
                    continue;

                predecessorSet.insert(predecessorGroup);
            }
        }

        std::copy(newGroup.rules.begin(), newGroup.rules.end(), std::back_inserter(allRuleDetails));

        for (unsigned successor : successorSet)
        {
            newGroup.successors.push_back(&positiveGroups[0] + successor);
        }

        for (unsigned predecessor : predecessorSet)
        {
            newGroup.predecessors.push_back(&positiveGroups[0] + predecessor);
        }
    }

    chaseMgmt = std::shared_ptr<ChaseMgmt>(new ChaseMgmt(allRuleDetails,
        typeChase, checkCyclicTerms,
        singleRuleToCheck,
        predIgnoreBlock));
}

bool SemiNaiverOrdered::executeGroup(
    std::vector<RuleExecutionDetails> &ruleset,
    std::vector<StatIteration> &costRules,
    bool blocked, unsigned long *timeout
)
{
    bool result = false;
    bool newDerivations = true;
    while (newDerivations)
    {
        newDerivations = false;
        int limitView = 0;

        size_t currentRule = 0;
        uint32_t rulesWithoutDerivation = 0; 

        size_t nRulesOnePass = 0;
        size_t lastIteration = 0;

        if ((typeChase == TypeChase::RESTRICTED_CHASE || typeChase == TypeChase::SUM_RESTRICTED_CHASE)) 
        {
            limitView = this->iteration == 0 ? 1 : this->iteration;
        }

        std::chrono::system_clock::time_point executionStart = std::chrono::system_clock::now();
        do 
        {
            std::chrono::system_clock::time_point iterationStart = std::chrono::system_clock::now();
            bool response = executeRule(ruleset[currentRule], iteration, limitView, NULL);

            result |= response;
            newDerivations |= response;

            if (timeout != NULL && *timeout != 0) 
            {
                std::chrono::duration<double> runDuration = std::chrono::system_clock::now() - startTime;
                if (runDuration.count() > *timeout) {
                    *timeout = 0;   // To indicate materialization was stopped because of timeout.
                    return result;
                }
            }

            std::chrono::duration<double> iterationDuration = std::chrono::system_clock::now() - iterationStart;
            StatIteration stat;
            stat.iteration = iteration;
            stat.rule = &ruleset[currentRule].rule;
            stat.time = iterationDuration.count() * 1000;
            stat.derived = response;
            costRules.push_back(stat);

            if (limitView > 0) 
            {
                // Don't use iteration here, because lastExecution determines which data we'll look at during the next round,
                // and limitView determines which data we are considering now. There should not be a gap.
                ruleset[currentRule].lastExecution = limitView;
                LOG(DEBUGL) << "Setting lastExecution of this rule to " << limitView;
            } 
            else 
            {
                ruleset[currentRule].lastExecution = iteration;
            }
            iteration++;

            if (checkCyclicTerms) {
                foundCyclicTerms = chaseMgmt->checkCyclicTerms(currentRule);
                if (foundCyclicTerms) {
                    LOG(DEBUGL) << "Found a cyclic term";
                    return result;
                }
            }

            if (response)
            {
                rulesWithoutDerivation = 0;
                nRulesOnePass++;
            }
            else
            {
                rulesWithoutDerivation++;
            }

            currentRule = (currentRule + 1) % ruleset.size();

            if (currentRule == 0) 
            {
                if (!blocked)
                    return result;
#ifdef DEBUG
                std::chrono::duration<double> sec = std::chrono::system_clock::now() - executionStart;
                LOG(DEBUGL) << "--Time round " << sec.count() * 1000 << " " << iteration;
                round_start = std::chrono::system_clock::now();
                //CODE FOR Statistics
                LOG(INFOL) << "Finish pass over the rules. Step=" << iteration << ". IDB RulesWithDerivation=" <<
                    nRulesOnePass << " out of " << ruleset.size() << " Derivations so far " << countAllIDBs();
                printCountAllIDBs("After step " + to_string(iteration) + ": ");
                nRulesOnePass = 0;

                //Get the top 10 rules in the last iteration
                std::sort(costRules.begin(), costRules.end());
                std::string out = "";
                int n = 0;
                for (const auto &exec : costRules) {
                    if (exec.iteration >= lastIteration) {
                        if (n < 10 || exec.derived) {
                            out += "Iteration " + to_string(exec.iteration) + " runtime " + to_string(exec.time);
                            out += " " + exec.rule->tostring(program, &layer) + " response " + to_string(exec.derived);
                            out += "\n";
                        }
                        n++;
                    }
                }
                LOG(DEBUGL) << "Rules with the highest cost\n\n" << out;
                lastIteration = iteration;
                //END CODE STATISTICS
#endif
            }
        } while (rulesWithoutDerivation != ruleset.size());
    }

    return result;
}

#define RUNMAT 1

void SemiNaiverOrdered::setActive(PositiveGroup *currentGroup)
{
    currentGroup->active = true;

    for (PositiveGroup *successorGroup : currentGroup->successors)
    {
        setActive(successorGroup);
    }
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

    std::vector<Rule> &allRules = program->getAllRules();
    std::cout << "#Rules: " << allRules.size() << std::endl;

    std::cout << "Computing positive reliances..." << std::endl;
    std::pair<RelianceGraph, RelianceGraph> relianceGraphs = computePositiveReliances(allRules);

    /*std::cout << "Positive reliances: " << std::endl;
    for (unsigned from = 0; from < relianceGraphs.first.edges.size(); ++from)
    {
        for (unsigned to :  relianceGraphs.first.edges[from])
        {
            std::cout << "positive reliance: " << from << " -> " << to << std::endl;
        }
    }*/

    std::cout << "Computing positive reliance groups..." << std::endl;
    RelianceGroupResult relianceGroups = computeRelianceGroups(relianceGraphs.first, relianceGraphs.second);

    /*for (vector<unsigned> &group : relianceGroups.groups)
    {
        if (group.size() < 2)
            continue;

        for (unsigned member : group)
        {
            std::cout << allRules[member].tostring(program, &layer) << std::endl;
        }
        std::cout << "------------------------" << std::endl;
    }

    std::cout << "Found " << relianceGroups.groups.size() << " groups." << std::endl;*/

    std::vector<StatIteration> costRules;

#if RUNMAT
    std::vector<PositiveGroup> positiveGroups;
    prepare(lastExecution, singleRule, allRules, relianceGraphs.first, relianceGraphs.second, relianceGroups, positiveGroups);

    std::deque<PositiveGroup *> positiveStack;

    for (PositiveGroup &currentGroup : positiveGroups)
    {
        for (const RuleExecutionDetails &details : currentGroup.rules)
        {
            if (details.nIDBs == 0)
            {
                positiveStack.push_front(&currentGroup);
                currentGroup.inQueue = true;
                currentGroup.triggered = true;

                setActive(&currentGroup);
            }
        }
    }

    PositiveGroup *firstBlockedGroup = nullptr;
    while (!positiveStack.empty())
    {
        PositiveGroup *currentGroup = positiveStack.front();
        positiveStack.pop_front();
        currentGroup->inQueue = false;

        bool hasActivePredecessors = false;
        for (PositiveGroup *predecessorGroup : currentGroup->predecessors)
        {
            if (predecessorGroup->active)
            {
                hasActivePredecessors = true;
                break;
            }
        }

        if (!hasActivePredecessors)
        {
            bool newDerivations = false;
            bool isBlocked = false;
            
            if (currentGroup->triggered)
            {
                for (PositiveGroup *blockingGroup : currentGroup->blockers)
                {
                    if (blockingGroup->active)
                    {
                        isBlocked = true;
                        break;
                    }
                }

                if (!isBlocked || currentGroup == firstBlockedGroup)
                {
                    newDerivations = executeGroup(currentGroup->rules, costRules, !isBlocked, timeout);
                }

                if (isBlocked && firstBlockedGroup == nullptr)
                {
                    firstBlockedGroup = currentGroup;
                }
                else if (currentGroup == firstBlockedGroup)
                {
                    firstBlockedGroup = nullptr;
                }
            }

            if (!isBlocked)
            {
                currentGroup->active = false;
            }
            else
            {
                positiveStack.push_back(currentGroup);
                currentGroup->inQueue = true;
            }

            if (newDerivations || !isBlocked)
            {
                for (PositiveGroup *successorGroup : currentGroup->successors)
                {
                    if (newDerivations)
                    {
                        successorGroup->triggered = true;
                    }

                    if (!successorGroup->inQueue)
                    {
                        positiveStack.push_front(successorGroup);   
                        successorGroup->inQueue = true;
                    }
                }
            }
        }
    }
#endif

    this->running = false;
}
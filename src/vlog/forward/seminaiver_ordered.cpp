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

std::vector<std::vector<unsigned>> SemiNaiverOrdered::computeRelianceGroups(
    const RelianceGraph &graph, const RelianceGraph &graphTransposed)
{
    std::vector<std::vector<unsigned>> result;
    
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
                result.push_back(currentGroup);
                currentGroup.clear();
            }
        }
    }

    return result;
}

void SemiNaiverOrdered::prepare(size_t lastExecution, int singleRuleToCheck, const std::vector<Rule> &allRules,
    const RelianceGraph &positiveGraph, const std::vector<std::vector<unsigned>> &groups,
    std::vector<PositiveGroup> &positiveGroups)
{
    positiveGroups.resize(groups.size());
    std::vector<RuleExecutionDetails> allRuleDetails;

    for (unsigned groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        const std::vector<unsigned> &group = groups[groupIndex];
        PositiveGroup &newGroup = positiveGroups[groupIndex];

        newGroup.rules.reserve(group.size());

        std::unordered_set<unsigned> successorSet;

        for (unsigned ruleIndex : group)
        {
            const Rule &currentRule = allRules[ruleIndex];
            newGroup.rules.emplace_back(currentRule, currentRule.getId());

            newGroup.rules.back().createExecutionPlans(checkCyclicTerms);
            newGroup.rules.back().calculateNVarsInHeadFromEDB();
            newGroup.rules.back().lastExecution = lastExecution;
        
            for (unsigned successor : positiveGraph.edges[ruleIndex])
            {
                successorSet.insert(successor);
            }
        }

        std::copy(newGroup.rules.begin(), newGroup.rules.end(), std::back_inserter(allRuleDetails));

        for (unsigned successor : successorSet)
        {
            newGroup.successors.push_back(&positiveGroups[0] + successor);
        }
    }

    chaseMgmt = std::shared_ptr<ChaseMgmt>(new ChaseMgmt(allRuleDetails,
        typeChase, checkCyclicTerms,
        singleRuleToCheck,
        predIgnoreBlock));
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
    this->iteration = iteration;
    this->startTime = std::chrono::system_clock::now();
    listDerivations.clear();

    std::vector<Rule> &allRules = program->getAllRules();

    std::cout << "Computing positive reliances..." << std::endl;
    std::pair<RelianceGraph, RelianceGraph> relianceGraphs = computePositiveReliances(allRules);

    std::cout << "Positive reliances: " << std::endl;
    for (unsigned from = 0; from < relianceGraphs.first.edges.size(); ++from)
    {
        for (unsigned to :  relianceGraphs.first.edges[from])
        {
            std::cout << "positive reliance: " << from << " -> " << to << std::endl;
        }
    }

    std::cout << "Computing positive reliance groups..." << std::endl;
    std::vector<std::vector<unsigned>> relianceGroups = computeRelianceGroups(relianceGraphs.first, relianceGraphs.second);

    for (vector<unsigned> &group : relianceGroups)
    {
        if (group.size() < 2)
            continue;

        for (unsigned member : group)
        {
            std::cout << allRules[member].tostring(program, &layer) << std::endl;
        }
        std::cout << "------------------------" << std::endl;
    }

    std::cout << "Found " << relianceGroups.size() << " groups." << std::endl;

    std::vector<StatIteration> costRules;

    std::vector<PositiveGroup> positiveGroups;
    prepare(lastExecution, singleRule, allRules, relianceGraphs.first, relianceGroups, positiveGroups);

    std::stack<PositiveGroup *> positiveStack;

    for (PositiveGroup &currentGroup : positiveGroups)
    {
        for (const RuleExecutionDetails &details : currentGroup.rules)
        {
            if (details.nIDBs == 0)
            {
                positiveStack.push(&currentGroup);
            }
        }
    }

    int limitView = 0;

    while (!positiveStack.empty())
    {
        PositiveGroup *currentGroup = positiveStack.top();
        positiveStack.pop();

        currentGroup->triggered = false;
        currentGroup->active = false;

        if ((typeChase == TypeChase::RESTRICTED_CHASE || typeChase == TypeChase::SUM_RESTRICTED_CHASE)) 
        {
            limitView = iteration == 0 ? 1 : iteration;
        }

        bool newDerivations = executeUntilSaturation(currentGroup->rules, costRules, limitView, true, timeout);

        if (!newDerivations)
            continue;

        for (PositiveGroup *successorGroup : currentGroup->successors)
        {
            if (!successorGroup->triggered)
            {
                successorGroup->triggered = true;
                positiveStack.push(successorGroup);
            }
        }
    }

    this->running = false;
}
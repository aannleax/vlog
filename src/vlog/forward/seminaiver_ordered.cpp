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

/*
    Test case:
    
    RelianceGraph test(9);
    RelianceGraph testTransposed(9);

    test.addEdge(0, 1); testTransposed.addEdge(1, 0);
    test.addEdge(1, 2); testTransposed.addEdge(2, 1);
    test.addEdge(2, 3); testTransposed.addEdge(3, 2);
    test.addEdge(3, 0); testTransposed.addEdge(0, 3);
    test.addEdge(2, 4); testTransposed.addEdge(4, 2);
    test.addEdge(4, 5); testTransposed.addEdge(5, 4);
    test.addEdge(5, 6); testTransposed.addEdge(6, 5);
    test.addEdge(6, 4); testTransposed.addEdge(4, 6);
    test.addEdge(7, 6); testTransposed.addEdge(6, 7);
    test.addEdge(7, 8); testTransposed.addEdge(8, 7); 

    relianceGraphs.first = test;
    relianceGraphs.second = testTransposed;
*/

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

void SemiNaiverOrdered::run(size_t lastIteration,
    size_t iteration,
    unsigned long *timeout,
    bool checkCyclicTerm,
    int singleRule,
    PredId_t predIgnoreBlock)
{
    std::cout << "Rules.tostring():" << std::endl;
    for (Rule &currentRule: program->getAllRules())
    {
        std::cout << currentRule.tostring() << std::endl;
    }

    std::cout << "Computing positive reliances..." << std::endl;
    std::pair<RelianceGraph, RelianceGraph> relianceGraphs = computePositiveReliances(program);

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
        for (unsigned member : group)
        {
            std::cout << member << " ";
        }
        std::cout << std::endl;
    }

    std::cout << "Found " << relianceGroups.size() << " groups." << std::endl;
}
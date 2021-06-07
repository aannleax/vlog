#ifndef _RELIENCES_H
#define _RELIENCES_H

#include <vector>

struct RelianceGraph
{
    std::vector<std::vector<unsigned>> Edges;

    RelianceGraph(unsigned NodeCount)
    {
        Edges.resize(NodeCount);
    } 

    void AddEdge(unsigned From, unsigned To)
    {
        Edges[From].push_back(To);
    }
};

#endif
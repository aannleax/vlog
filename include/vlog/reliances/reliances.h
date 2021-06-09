#ifndef _RELIENCES_H
#define _RELIENCES_H

#include "vlog/concepts.h"

#include <vector>
#include <limits>

#define NOT_ASSIGNED std::numeric_limits<int64_t>::max()

struct RelianceGraph
{
    std::vector<std::vector<unsigned>> edges;

    RelianceGraph(unsigned nodeCount)
    {
        edges.resize(nodeCount);
    } 

    void addEdge(unsigned from, unsigned to)
    {
        edges[from].push_back(to);
    }
};

struct Assignment
{
    int64_t group = NOT_ASSIGNED;
    int64_t value = NOT_ASSIGNED;
};

std::pair<RelianceGraph, RelianceGraph> computePositiveReliances(Program *program);

#endif
#ifndef _RELIENCES_H
#define _RELIENCES_H

#include "vlog/concepts.h"

#include <vector>
#include <limits>
#include <fstream>
#include <string>

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

    void saveCSV(const std::string &filename) const
    {
        std::ofstream stream(filename);

        for (unsigned from = 0; from < edges.size(); ++from)
        {
            for (unsigned to : edges[from])
            {
                stream << from << "," << to << std::endl;
            }
        }
    }

    bool containsEdge(unsigned from, unsigned to) const
    {
        if (edges[from].size() > 0)
        {
            auto iter = std::find(edges[from].begin(), edges[from].end(), to);
            return iter != edges[from].end();
        }

        return false;
    }
};

std::pair<RelianceGraph, RelianceGraph> computePositiveReliances(std::vector<Rule> &rules);
std::pair<RelianceGraph, RelianceGraph> computeRestrainReliances(std::vector<Rule> &rules);
unsigned DEBUGcountFakePositiveReliances(const std::vector<Rule> &rules, const RelianceGraph &positiveGraph);

#endif
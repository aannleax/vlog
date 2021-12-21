#include <vlog/reliances/reliances.h>
#include <vlog/concepts.h>
#include <vlog/edbconf.h>
#include <vlog/edb.h>

#include <iostream>
#include <string>
#include <vector>

struct TestCase
{
    enum class Type
    {
        Positive, Restraint
    } type;

    std::string name;
    SimpleGraph expected;

    TestCase(Type type, const std::string name, 
        std::vector<std::vector<size_t>> edges)
    {
        this->type = type;
        this->name = name;

        size_t nodeCount = 0;
        for (const auto &vec : edges)
        {
            for (size_t entry : vec)
            {
                if (entry > nodeCount)
                {
                    nodeCount = entry;
                }
            }
        }

        expected = SimpleGraph(nodeCount + 1);
        expected.edges = edges;
    }
};

void performTests(const std::string &ruleFolder);
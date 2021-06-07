#include "vlog/reliances/reliances.h"
#include "vlog/concepts.h"

#include <vector>
#include <utility>

std::pair<RelianceGraph, RelianceGraph> computePositiveReliances(Program *program)
{
    std::vector<Rule> rules = program->getAllRules();
    RelianceGraph result(rules.size()), resultTransposed(rules.size());



    return std::make_pair(result, resultTransposed);
}

bool positiveReliance()
{
    bool result = false;

  

    return result;
}
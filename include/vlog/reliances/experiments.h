#include <vlog/reliances/reliances.h>
#include <vlog/concepts.h>
#include <vlog/edbconf.h>
#include <vlog/edb.h>
#include <vlog/cycles/checker.h>

#include <iostream>
#include <string>

void experimentCoreStratified(const std::string &rulesPath, bool pieceDecomposition, RelianceStrategy strat, unsigned timeoutMilliSeconds);
void experimentCycles(const std::string &rulePath, const std::string &algorithm, bool splitPositive, unsigned timeoutMilliSeconds);
#include <vlog/reliances/experiments.h>

void experimentCoreStratified(const std::string &rulesPath, bool pieceDecomposition)
{
    std::cout << "Launched coreStratified experiment with parameters " << '\n';
    std::cout << "\t" << rulesPath << '\n';
    std::cout << "\t" << ((pieceDecomposition) ? "true" : "false") << std::endl;

    EDBConf emptyConf("", false);
    EDBLayer edbLayer(emptyConf, false);

    Program program(&edbLayer);
    std::string errorString = program.readFromFile(rulesPath, false);
    if (!errorString.empty()) {
        LOG(ERRORL) << errorString;
        return;
    }

    const std::vector<Rule> &allOriginalRules = program.getAllRules();

    std::vector<Rule> allPieceDecomposedRules;
    if (pieceDecomposition)
    {
        for (const Rule &currentRule : allOriginalRules)
        {
            splitIntoPieces(currentRule, allPieceDecomposedRules);
        }
    }

    const std::vector<Rule> &allRules = (pieceDecomposition) ? allPieceDecomposedRules : allOriginalRules;
    
    std::cout << "#Rules: " << allOriginalRules.size() << '\n';
    if (pieceDecomposition)
        std::cout << "#PieceRules: " << allPieceDecomposedRules.size() << '\n';

    std::cout << "Computing positive reliances..." << '\n';
    std::chrono::system_clock::time_point relianceStart = std::chrono::system_clock::now();
    std::pair<SimpleGraph, SimpleGraph> positiveGraphs = computePositiveReliances(allRules);
    std::cout << "Time Positive: " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - relianceStart).count() / 1000.0 << '\n';

    std::cout << "Computing restraint reliances..." << '\n';
    relianceStart = std::chrono::system_clock::now();
    std::pair<SimpleGraph, SimpleGraph> restrainingGraphs = computeRestrainReliances(allRules);
    std::cout << "Time Restraint: " << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - relianceStart).count() / 1000.0 << '\n';

    std::pair<SimpleGraph, SimpleGraph> unionGraphs = combineGraphs(positiveGraphs.first, restrainingGraphs.first);

    bool coreStratified = isCoreStratified(unionGraphs.first, unionGraphs.second, restrainingGraphs.first);

    std::cout << "Core-Stratified: " << ((coreStratified) ? "yes" : "no") << '\n';
}
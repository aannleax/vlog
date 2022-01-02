#include <vlog/reliances/experiments.h>

void experimentCoreStratified(const std::string &rulesPath, bool pieceDecomposition, RelianceStrategy strat, unsigned timeoutMilliSeconds)
{
    std::cout << "Launched coreStratified experiment with parameters " << '\n';
    std::cout << "\t" << "Path: " << rulesPath << '\n';
    std::cout << "\t" << "Piece: " << ((pieceDecomposition) ? "true" : "false") << '\n';
    std::cout << "\t" <<  "Strat: " << strat << std::endl;

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

    RelianceComputationResult positiveResult = computePositiveReliances(allRules, strat, timeoutMilliSeconds);
    std::pair<SimpleGraph, SimpleGraph> positiveGraphs = positiveResult.graphs;

    RelianceComputationResult restrainResult = computeRestrainReliances(allRules, strat, timeoutMilliSeconds);
    std::pair<SimpleGraph, SimpleGraph> restrainingGraphs = restrainResult.graphs;
    
    bool timeout = positiveResult.timeout || restrainResult.timeout;

    std::cout << "Calls-Positive: " << positiveResult.numberOfCalls << '\n';
    std::cout << "Calls-Restraint: " << restrainResult.numberOfCalls << '\n';
    std::cout << "Calls-Overall: " << positiveResult.numberOfCalls + restrainResult.numberOfCalls << '\n';
    
    std::cout << "Longest-Positive: " << positiveResult.timeLongestPairMicro / 1000.0 << '\n';
    std::cout << "Longest-Pair-Positive: " << positiveResult.longestPairString << '\n';
    std::cout << "Longest-Restraint: " << restrainResult.timeLongestPairMicro / 1000.0 << '\n';
    std::cout << "Longest-Pair-Restraint: " << restrainResult.longestPairString << '\n';
    std::cout << "Longest-Overall: " << (positiveResult.timeLongestPairMicro + restrainResult.timeLongestPairMicro) / 1000.0 << '\n';

    if (!timeout)
    {
        std::cout << "Time-Positive: " << positiveResult.timeMilliSeconds << '\n';
        std::cout << "Time-Restraint: " << restrainResult.timeMilliSeconds << '\n';
        std::cout << "Time-Overall: " << positiveResult.timeMilliSeconds + restrainResult.timeMilliSeconds << '\n';

        std::pair<SimpleGraph, SimpleGraph> unionGraphs = combineGraphs(positiveGraphs.first, restrainingGraphs.first);
        CoreStratifiedResult coreStratifiedResult = isCoreStratified(unionGraphs.first, unionGraphs.second, restrainingGraphs.first);
        bool coreStratified = coreStratifiedResult.stratified;
        std::cout << "Core-Stratified: " << ((coreStratified) ? "1" : "0") << '\n';

        if (!coreStratified)
        {
            std::cout << "NumberRestrainedGroups: " << coreStratifiedResult.numberOfRestrainedGroups << '\n';
            std::cout << "BiggestRestrainedGroup-Abs: " << coreStratifiedResult.biggestRestrainedGroupSize << '\n';
            std::cout << "BiggestRestrainedGroup-Rel: " << (coreStratifiedResult.biggestRestrainedGroupSize / (float)allRules.size()) << '\n';
            std::cout << "RulesInRestrainedGroups-Rel: " << (coreStratifiedResult.numberOfRulesInRestrainedGroups / (float)allRules.size()) << '\n';
        }
    }

    std::cout << "Timeout: " << ((timeout) ? "1" : "0") << '\n';
}
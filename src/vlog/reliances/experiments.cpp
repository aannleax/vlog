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

void experimentCycles(const std::string &rulesPath, const std::string &algorithm, bool splitPositive, unsigned timeoutMilliSeconds)
{
    std::cout << "Launched Cycles experiment with parameters " << '\n';
    std::cout << "\t" << "Algorithm: " << algorithm<< '\n';

    EDBConf emptyConf("", false);
    EDBLayer edbLayer(emptyConf, false);

    Program initialProgram(&edbLayer);
    std::string errorString = initialProgram.readFromFile(rulesPath, false);
    if (!errorString.empty()) {
        LOG(ERRORL) << errorString;
        return;
    }

    const std::vector<Rule> &allOriginalRules = initialProgram.getAllRules();
    RelianceGroupResult positiveGroupsResult;
    double timeRelianceMilliseconds = 0.0;
    if (splitPositive)
    {
        auto relianceStart = std::chrono::system_clock::now();

        RelianceComputationResult positiveResult = computePositiveReliances(allOriginalRules, RelianceStrategy::Full, timeoutMilliSeconds);
        std::pair<SimpleGraph, SimpleGraph> positiveGraphs = positiveResult.graphs;

        timeRelianceMilliseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - relianceStart).count() / 1000.0;
    
        if (positiveResult.timeout)
        {
            std::cout << "Timeout: 1" << '\n';
            return;
        }
        else
        {
            std::cout << "Reliance-Time: " << timeRelianceMilliseconds << " ms" << '\n';
        }

        positiveGroupsResult = computeRelianceGroups(positiveGraphs.first, positiveGraphs.second);    
    }

    auto cycleStart = std::chrono::system_clock::now();
    bool result = true;
    
    if (splitPositive)
    {
        for (const std::vector<unsigned> &group : positiveGroupsResult.groups)
        {
            Program currentProgram(&edbLayer);
            bool containsExisitential = false;
            for (unsigned ruleIndex : group)
            {
                const Rule &currentRule = allOriginalRules[ruleIndex];

                if (currentRule.isExistential())
                    containsExisitential = true;

                currentProgram.addRule(currentRule.getHeads(), currentRule.getBody());
            }

            if (!containsExisitential)
                continue;

            int checkResult = Checker::check(currentProgram, algorithm, edbLayer);
        
            if (checkResult == 0)
                result = false;
        }
    }
    else
    {
        int checkResult = Checker::check(initialProgram, algorithm, edbLayer);

        result = (checkResult == 1);
    }
    

    double timeCycleMilliseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - cycleStart).count() / 1000.0;
    double timeOverallMilliseconds = timeRelianceMilliseconds + timeCycleMilliseconds;

    std::cout << "Acyclic: " << ((result) ? "1" : "0") << '\n';
    std::cout << "Cycle-Time: " << timeCycleMilliseconds << " ms" << '\n';
    std::cout << "Overall-Time: " << timeOverallMilliseconds << " ms" << '\n';
    std::cout << "Timeout: 0" << '\n';
}
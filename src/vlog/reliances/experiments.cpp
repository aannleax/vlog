#include <vlog/reliances/experiments.h>

SimpleGraph createSubgraph(const SimpleGraph &originalGraph, std::vector<unsigned> component)
{
    SimpleGraph result(originalGraph.numberOfInitialNodes);

    std::sort(component.begin(), component.end());

    for (unsigned node : component)
    {
        for (unsigned successor : originalGraph.edges[node])
        {
            if (!std::binary_search(component.begin(), component.end(), successor))
                continue;

            result.addEdge(node, successor);
        }
    } 

    return result;
}

size_t minIndex(const std::vector<size_t> &distances, const std::vector<unsigned> &considered)
{
    unsigned minIndex = 0; 
    size_t minDistance = std::numeric_limits<size_t>::max();

    for (unsigned index = 0; index < distances.size(); ++index)
    {
        if (distances[index] < minDistance && considered[index] == 0)
        {
            minDistance = distances[index];
            minIndex = index;
        }
    }

    return minIndex;
}

void printShortestCycle(const SimpleGraph &positiveGraph, const SimpleGraph &positiveGraphTransposed,
    const SimpleGraph &restraintGraph, const SimpleGraph &restraintgraphTransposed,
    const std::vector<unsigned> &component,
    const std::vector<Rule> &rules, Program *program, EDBLayer *db)
{
    SimpleGraph positiveSubGraph = createSubgraph(positiveGraph, component);
    SimpleGraph restraintSubGraph = createSubgraph(restraintGraph, component);

    std::cout << "print cycles?" << component.size() << std::endl;

    std::vector<unsigned> smallestCycle;

    for (unsigned sourceNode : component)
    {
        if (restraintgraphTransposed.edges[sourceNode].size() == 0)
            continue;

        std::vector<size_t> distances;
        std::vector<unsigned> predecessors;
        std::vector<unsigned> considered;

        distances.resize(positiveGraph.numberOfInitialNodes, std::numeric_limits<size_t>::max());
        predecessors.resize(positiveGraph.numberOfInitialNodes, std::numeric_limits<unsigned>::max());
        considered.resize(positiveGraph.numberOfInitialNodes, 0);
    
        distances[sourceNode] = 0;

        for (size_t count = 0; count < component.size() - 1; ++count)
        {
            unsigned currentNode = minIndex(distances, considered);
            if (distances[currentNode] == std::numeric_limits<size_t>::max())
                break;

            considered[currentNode] = 1;

            for (unsigned adjacent : positiveSubGraph.edges[currentNode])
            {
                if (considered[adjacent] == 0 && distances[currentNode] + 1 < distances[adjacent])
                {
                    distances[adjacent] = distances[currentNode] + 1;
                    predecessors[adjacent] = currentNode;
                }
            }

            for (unsigned adjacent : restraintSubGraph.edges[currentNode])
            {
                if (considered[adjacent] == 0 && distances[currentNode] + 1 < distances[adjacent])
                {
                    distances[adjacent] = distances[currentNode] + 1;
                    predecessors[adjacent] = currentNode;
                }
            }
        }

        unsigned minPred = 0;
        unsigned minPredDistance = std::numeric_limits<unsigned>::max();
        for (unsigned pred : restraintgraphTransposed.edges[sourceNode])
        {
            if (distances[pred] < minPredDistance)
            {
                minPredDistance = distances[pred];
                minPred = pred;
            }
        }

        std::vector<unsigned> currentCycle;
        currentCycle.push_back(sourceNode);

        if (minPred == sourceNode)
        {
            smallestCycle = currentCycle;
            break;
        }

        unsigned currentPred = minPred;
        bool isSmaller = true;
        while (currentPred != sourceNode)
        {
            currentCycle.push_back(currentPred);
           
            if (smallestCycle.size() > 2 && smallestCycle.size() != 0 && currentCycle.size() > smallestCycle.size())
            {
                isSmaller = false;
                break;
            }

            currentPred = predecessors[currentPred];
        }

        if (isSmaller)
            smallestCycle = currentCycle; 
    }

    for (unsigned cycleIndex = 0; cycleIndex < smallestCycle.size(); ++cycleIndex)
    {
        std::cout << (cycleIndex + 1) << " - " << rules[smallestCycle[cycleIndex]].tostring(program, db) << '\n';
    }
}

void experimentCoreStratified(const std::string &rulesPath, bool pieceDecomposition, RelianceStrategy strat, unsigned timeoutMilliSeconds, bool printCycles, bool saveGraphs)
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

     if (!positiveResult.timeout)
    {
        std::cout << "Time-Positive: " << positiveResult.timeMilliSeconds << '\n';
    }   

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

    if (!restrainResult.timeout)
    {
        std::cout << "Time-Restraint: " << restrainResult.timeMilliSeconds << '\n';
    } 

    if (!timeout)
    {
        if (saveGraphs)
        {
            positiveGraphs.first.saveCSV(rulesPath + ".pos");
            restrainingGraphs.first.saveCSV(rulesPath + ".res");
        }

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
        
            if (printCycles)
            {
                printShortestCycle(positiveGraphs.first, positiveGraphs.second, restrainingGraphs.first, restrainingGraphs.second, 
                    coreStratifiedResult.smallestRestrainedComponent, allRules, &program, &edbLayer);
            }
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

    RelianceComputationResult positiveResult;

    if (splitPositive)
    {
        auto relianceStart = std::chrono::system_clock::now();

        positiveResult = computePositiveReliances(allOriginalRules, RelianceStrategy::Full, timeoutMilliSeconds);
        std::pair<SimpleGraph, SimpleGraph> &positiveGraphs = positiveResult.graphs;

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

    std::vector<std::string> ruleStrings;
    ruleStrings.reserve(allOriginalRules.size());
    for (const Rule &currentRule : allOriginalRules)
    {
        ruleStrings.push_back(currentRule.toprettystring(&initialProgram, &edbLayer));
    }

    auto cycleStart = std::chrono::system_clock::now();
    bool result = true;
    
    if (splitPositive)
    {
        for (const std::vector<unsigned> &group : positiveGroupsResult.groups)
        {
            if (group.size() == 1 && !positiveResult.graphs.first.containsEdge(group[0], group[0]))
                continue;

            EDBConf currentEmptyConf("", false);
            EDBLayer currentEdbLayer(emptyConf, false);

            Program currentProgram(&currentEdbLayer);
            bool containsExisitential = false;
            for (unsigned ruleIndex : group)
            //for (unsigned ruleIndex = 0; ruleIndex < allOriginalRules.size(); ++ruleIndex)
            {
                const Rule &currentRule = allOriginalRules[ruleIndex];

                if (currentRule.isExistential())
                    containsExisitential = true;

                currentProgram.readFromString(ruleStrings[ruleIndex], false);
                //currentProgram.addRule(currentRule.getHeads(), currentRule.getBody());
            }

            if (!containsExisitential)
                continue;

            int checkResult = Checker::check(currentProgram, algorithm, currentEdbLayer);
        
            if (checkResult == 0)
            {
                result = false;
                break;
            }
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

void experimentGRD(const std::string &rulesPath, unsigned timeoutMilliSeconds)
{
    EDBConf emptyConf("", false);
    EDBLayer edbLayer(emptyConf, false);

    Program program(&edbLayer);
    std::string errorString = program.readFromFile(rulesPath, false);
    if (!errorString.empty()) {
        LOG(ERRORL) << errorString;
        return;
    }

    const std::vector<Rule> &allRules = program.getAllRules();

    auto experimentStart = std::chrono::system_clock::now();
    
    RelianceComputationResult positiveResult = computePositiveReliances(allRules, RelianceStrategy::Full, timeoutMilliSeconds);
    
    if (positiveResult.timeout)
    {
        std::cout << "Timeout: 1" << '\n';
        return;
    }
    
    std::pair<SimpleGraph, SimpleGraph> positiveGraphs = positiveResult.graphs;
    RelianceGroupResult positiveGroupsResult = computeRelianceGroups(positiveGraphs.first, positiveGraphs.second);  

    bool isAcyclic = true;
    for (const std::vector<unsigned> &group : positiveGroupsResult.groups)
    {
        if (group.size() > 1)
        {
            isAcyclic = false;
            break;
        }
        else
        {
            if (positiveGraphs.first.containsEdge(group[0], group[0]))
            {
                isAcyclic = false;
                break;
            }
        }
    }   

    double timeMilliSeconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - experimentStart).count() / 1000.0;   

    size_t totalNumberOfEdges = 0;
    for (const auto &successors : positiveGraphs.first.edges)
    {
        totalNumberOfEdges += successors.size();
    }

    std::cout << "Acyclic: " << (isAcyclic ? "1" : "0") << '\n';
    std::cout << "NumberOfEdges: " << totalNumberOfEdges << '\n';
    std::cout << "Time: " << timeMilliSeconds << " ms" << '\n'; 
    std::cout << "Time-Positive: " << positiveResult.timeMilliSeconds << " ms" << '\n';
}

void printAllCyclesOWL(const std::string &inputPath)
{
    const std::string postfix = ".owl.rules";
    const std::string postfixPos = "pos";
    const std::string postfixRes = "res";
    const unsigned fileMax = 797;
    const unsigned filenameLength = 5;

    struct RuleComponent {
        std::vector<Rule> rules;
        SimpleGraph positiveGraph, restraintGraph;
    };

    auto literalsSameForm = [&] (const vector<Literal> &literalsA, const vector<Literal> &literalsB) -> bool {
        if (literalsA.size() != literalsB.size())
            return false;
    
        for (int literalIndex = 0; literalIndex < literalsA.size(); ++literalIndex)
        {
            const Literal &literalA = literalsA[literalIndex];
            const Literal &literalB = literalsB[literalIndex];

            for (unsigned termIndex = 0; termIndex < literalA.getTupleSize(); ++termIndex)
            {
                VTerm currentTermA = literalA.getTermAtPos(termIndex);
                VTerm currentTermB = literalB.getTermAtPos(termIndex);

                if (currentTermA != currentTermB)
                    return false;
            }
        }   

        return true;
    };

    auto rulesSameForm = [&] (const Rule &ruleA, const Rule &ruleB) -> bool {
        return literalsSameForm(ruleA.getBody(), ruleB.getBody()) 
            && literalsSameForm(ruleA.getHeads(), ruleB.getHeads());
    };

    auto remapGraph = [] (const std::vector<Rule> &rules, const SimpleGraph &graph) -> SimpleGraph {
        std::unordered_map<unsigned, unsigned> ruleMapping;

        for (unsigned ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex)
        {
            ruleMapping[rules[ruleIndex].getId()] = ruleIndex;
        }

        SimpleGraph result(rules.size());

        for (unsigned ruleIndex = 0; ruleIndex < rules.size(); ++ruleIndex)
        {
            for (unsigned successor : graph.edges[rules[ruleIndex].getId()])
            {
                if (ruleMapping.find(successor) != ruleMapping.end())
                    result.addEdge(ruleIndex, ruleMapping[successor]);
            }
        }

        return result;
    };

    auto sameSuccessors = [&] (
        unsigned ruleAIndex, unsigned ruleBIndex,
        const SimpleGraph &graphA, const SimpleGraph &graphB,
        const std::vector<Rule> &rulesA, const std::vector<Rule> &rulesB) -> bool {

        if (graphA.edges[ruleAIndex].size() != graphB.edges[ruleBIndex].size())
            return false;

        for (unsigned ruleASuccessorIndex : graphA.edges[ruleAIndex])
        {
            bool found = false;

            for (unsigned ruleBSuccessorIndex : graphB.edges[ruleBIndex])
            {
                if (rulesSameForm(rulesA[ruleASuccessorIndex], rulesB[ruleBSuccessorIndex]))
                {
                    found = true;
                    break;
                }
            }

            if (!found)
                return false;
        }

        for (unsigned ruleBSuccessorIndex : graphB.edges[ruleBIndex])
        {
            bool found = false;

            for (unsigned ruleASuccessorIndex : graphA.edges[ruleAIndex])
            {
                if (rulesSameForm(rulesA[ruleASuccessorIndex], rulesB[ruleBSuccessorIndex]))
                {
                    found = true;
                    break;
                }
            }

            if (!found)
                return false;
        }

        return true;
    };

    auto componentSameForm = [&] (const RuleComponent &componentA, 
        const std::vector<unsigned> componentBIndizes, const std::vector<Rule> &allRulesB,
        const SimpleGraph &positiveReliancesB, const SimpleGraph &restraintReliancesB) -> bool {
        
        if (componentA.rules.size() != componentBIndizes.size())
            return false;

        for (unsigned ruleAIndex = 0; ruleAIndex < componentA.rules.size(); ++ruleAIndex)
        {
            const Rule &ruleA = componentA.rules[ruleAIndex];

            bool foundMatch = false;
            for (unsigned ruleBIndex : componentBIndizes)
            {
                const Rule &ruleB = allRulesB[ruleBIndex];

                if (!rulesSameForm(ruleA, ruleB))
                    continue;
                
                if (!sameSuccessors(ruleAIndex, ruleBIndex, 
                    componentA.positiveGraph, positiveReliancesB,
                    componentA.rules, allRulesB))
                    continue;

                if (!sameSuccessors(ruleAIndex, ruleBIndex, 
                    componentA.restraintGraph, restraintReliancesB,
                    componentA.rules, allRulesB))
                    continue;

                foundMatch = true;
                break;
            }

            if (!foundMatch)
                return false;
        }

        return true;
    };

    std::vector<RuleComponent> allComponents;

    for (unsigned fileNumber = 0; fileNumber <= fileMax; ++fileNumber)
    {
        std::string filename = std::to_string(fileNumber);
        while (filename.size() < filenameLength)
        {
            filename.insert(filename.begin(), '0');
        }

        std::string filePath = inputPath + "/" + filename + postfix;

        std::ifstream fileStream(filePath);
        if (!fileStream.good())
            continue;

        EDBConf emptyConf("", false);
        EDBLayer edbLayer(emptyConf, false);

        Program program(&edbLayer);
        std::string errorString = program.readFromFile(filePath, false);
        if (!errorString.empty()) {
            continue;
        }

        const std::vector<Rule> &allOriginalRules = program.getAllRules();

        std::vector<Rule> allRules;

        for (const Rule &currentRule : allOriginalRules)
        {
            splitIntoPieces(currentRule, allRules);
        }

        // RelianceComputationResult positiveResult = computePositiveReliances(allRules, RelianceStrategy::Full, 0);
        // RelianceComputationResult restrainResult = computeRestrainReliances(allRules, RelianceStrategy::Full, 0);
        // std::pair<SimpleGraph, SimpleGraph> positiveGraphs = positiveResult.graphs;
        // std::pair<SimpleGraph, SimpleGraph> restrainingGraphs = restrainResult.graphs;

        std::pair<SimpleGraph, SimpleGraph> positiveGraphs = SimpleGraph::loadAndTransposed(filePath + postfixPos);
        std::pair<SimpleGraph, SimpleGraph> restrainingGraphs = SimpleGraph::loadAndTransposed(filePath + postfixRes);

        std::pair<SimpleGraph, SimpleGraph> unionGraphs = combineGraphs(positiveGraphs.first, restrainingGraphs.first);
        RelianceGroupResult groupResult = computeRelianceGroups(unionGraphs.first, unionGraphs.second);

        for (const std::vector<unsigned> &group : groupResult.groups)
        {
            if (group.size() <= 1)
                continue;

            bool alreadyPresent = false;
            for (const RuleComponent &presentComponent : allComponents)
            {
                if (componentSameForm(presentComponent, group, allRules, positiveGraphs.first, restrainingGraphs.first))
                {
                    alreadyPresent = true;
                    break;
                }
            }

            if (!alreadyPresent)
            {
                RuleComponent newComponent;
                for (unsigned ruleIndex : group)
                {
                    newComponent.rules.push_back(allRules[ruleIndex]);
                }

                newComponent.positiveGraph = remapGraph(newComponent.rules, positiveGraphs.first);
                newComponent.restraintGraph = remapGraph(newComponent.rules, restrainingGraphs.first);

                allComponents.push_back(newComponent);
            }
        }
    }

    auto componentSortFunction = [] (const RuleComponent &componentA, const RuleComponent &componentB) -> bool {
        return componentA.rules.size() < componentB.rules.size();
    };

    std::sort(allComponents.begin(), allComponents.end(), componentSortFunction);

    auto componentToString = [] (const RuleComponent &component) -> std::string {
        std::string result;

        for (const Rule &rule : component.rules)
        {
            result += rule.tostring() + '\n';
        }

        return result;
    };

    std::cout << "Number of components: " << allComponents.size() << std::endl;

    for (const RuleComponent &ruleComponent : allComponents)
    {
        std::cout << "---------------------------" << std::endl;
        std::cout << componentToString(ruleComponent) << std::endl;
    }
}
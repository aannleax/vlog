
#include <vlog/reliances/tests.h>

bool graphContained(const SimpleGraph &left, const SimpleGraph &right)
{
    for (size_t from = 0; from < left.numberOfInitialNodes; ++from)
    {
        for (size_t to : left.edges[from])
        {
            if (!right.containsEdge(from, to))
                return false;
        }
    }

    return true;
}

bool performSingleTest(const std::string &ruleFolder, const TestCase &test)
{
    EDBConf emptyConf("", false);
    EDBLayer edbLayer(emptyConf, false);

    Program program(&edbLayer);
    std::string errorString = program.readFromFile(ruleFolder + "/" + test.name, false);
    if (!errorString.empty()) {
        LOG(ERRORL) << errorString;
        return false;
    }

    const std::vector<Rule> &allRules = program.getAllRules();

    std::pair<SimpleGraph, SimpleGraph> graphs;

    if (test.type == TestCase::Type::Positive)
    {
        RelianceComputationResult compResult = computePositiveReliances(allRules);
        graphs = compResult.graphs;
    }
    else 
    {
        RelianceComputationResult compResult = computeRestrainReliances(allRules);
        graphs = compResult.graphs;
    }

    return (graphContained(graphs.first, test.expected) && graphContained(test.expected, graphs.first));
}

typedef std::vector<std::vector<size_t>> Edges;

void performTests(const std::string &ruleFolder)
{
    std::vector<TestCase> cases;
    cases.emplace_back(TestCase::Type::Positive, "pos_basic.dl", Edges{{1}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_basic_2.dl", Edges{{1}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_thesis.dl", Edges{{1}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_null_1.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_null_2.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_phi2Ia.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_psi1Ia_phi1.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_psi1Ia_phi1phi22.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_psi1Ia_phi22.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_psi2Ib_phi1.dl", Edges{{}, {0}});
    cases.emplace_back(TestCase::Type::Positive, "pos_psi2Ib_phi22.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_psi2Ib_psi1.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_unif_1.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Positive, "pos_unif_2.dl", Edges{{}, {}});

    cases.emplace_back(TestCase::Type::Restraint, "res_basic_1.dl", Edges{{}, {0}});
    cases.emplace_back(TestCase::Type::Restraint, "res_basic_2.dl", Edges{{}, {0}});
    cases.emplace_back(TestCase::Type::Restraint, "res_null_1.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Restraint, "res_null_2.dl", Edges{{1}, {}});
    cases.emplace_back(TestCase::Type::Restraint, "res_null_3.dl", Edges{{1, 0}, {1}}); // Should change when self-restraints are implemented
    cases.emplace_back(TestCase::Type::Restraint, "res_null_4.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Restraint, "res_psi1Ibm_psi2.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Restraint, "res_psi2Iam_phi2.dl", Edges{{}, {}});
    cases.emplace_back(TestCase::Type::Restraint, "res_psi1Ibm_phi1phi2.dl", Edges{{}, {1}});
    cases.emplace_back(TestCase::Type::Restraint, "res_psi1Ibm_phi1phi2psi22.dl", Edges{{}, {0}}); // Should change when self-restraints are implemented
    cases.emplace_back(TestCase::Type::Restraint, "res_self_markus.dl", Edges{{}}); // Should change when self-restraints are implemented
    cases.emplace_back(TestCase::Type::Restraint, "res_self_trivial.dl", Edges{{0}}); // Should change when self-restraints are implemented
    cases.emplace_back(TestCase::Type::Restraint, "res_self_twice.dl", Edges{{0}});
 
    size_t numberOfFailedTests = 0;

    for (const TestCase &test : cases)
    {
        if (!performSingleTest(ruleFolder, test))
        {
            std::cout << test.name << " failed." << std::endl;

            ++numberOfFailedTests;
        }
    }

    std::cout << (cases.size() - numberOfFailedTests) << "/" << cases.size() << " passed." << std::endl;
}
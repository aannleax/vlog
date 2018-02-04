//VLog
#include <vlog/reasoner.h>
#include <vlog/materialization.h>
#include <vlog/seminaiver.h>
#include <vlog/edbconf.h>
#include <vlog/edb.h>
#include <vlog/webinterface.h>
#include <vlog/fcinttable.h>
#include <vlog/exporter.h>

//Used to load a Trident KB
#include <vlog/trident/tridenttable.h>
#include <launcher/vloglayer.h>
#include <trident/loader.h>
#include <kognac/utils.h>
#include <kognac/progargs.h>
#include <layers/TridentLayer.hpp>

//RDF3X
#include <cts/parser/SPARQLLexer.hpp>
#include <cts/parser/SPARQLParser.hpp>
#include <cts/infra/QueryGraph.hpp>
#include <cts/semana/SemanticAnalysis.hpp>
#include <cts/plangen/PlanGen.hpp>
#include <cts/codegen/CodeGen.hpp>
#include <rts/runtime/Runtime.hpp>
#include <rts/runtime/QueryDict.hpp>
#include <rts/operator/Operator.hpp>
#include <rts/operator/PlanPrinter.hpp>
#include <rts/operator/ResultsPrinter.hpp>

#include <iostream>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <chrono>
#include <thread>

using namespace std;

void printHelp(const char *programName, ProgramArgs &desc) {
    cout << "Usage: " << programName << " <command> [options]" << endl << endl;
    cout << "Possible commands:" << endl;
    cout << "help\t\t produce help message." << endl;
    cout << "mat\t\t perform a full materialization." << endl;
    cout << "query\t\t execute a SPARQL query." << endl;
    cout << "queryLiteral\t\t execute a Literal query." << endl;
    cout << "server\t\t starts in server mode." << endl;
    cout << "load\t\t load a Trident KB." << endl;
    cout << "gentq\t\t generate training queries from rules file." << endl;
    cout << "lookup\t\t lookup for values in the dictionary." << endl << endl;

    cout << desc.tostring() << endl;
}

inline void printErrorMsg(const char *msg) {
    cout << endl << "*** ERROR: " << msg << "***" << endl << endl
        << "Please type the subcommand \"help\" for instructions (e.g. Vlog help)."
        << endl;
}

bool checkParams(ProgramArgs &vm, int argc, const char** argv) {

    string cmd;
    if (argc < 2) {
        printErrorMsg("Command is missing!");
        return false;
    } else {
        cmd = argv[1];
    }

    if (cmd != "help" && cmd != "query" && cmd != "lookup" && cmd != "load" && cmd != "queryLiteral"
            && cmd != "mat" && cmd != "rulesgraph" && cmd != "server" && cmd != "gentq") {
        printErrorMsg(
                (string("The command \"") + cmd + string("\" is unknown.")).c_str());
        return false;
    }

    if (cmd == "help") {
        printHelp(argv[0], vm);
        return false;
    } else {
        /*** Check specific parameters ***/
        if (cmd == "query" || cmd == "queryLiteral") {
            string queryFile = vm["query"].as<string>();
            if (cmd == "query" && (queryFile == ""  || !Utils::exists(queryFile))) {
                printErrorMsg(
                        (string("The file ") + queryFile
                         + string(" doesn't exist.")).c_str());
                return false;
            }

            if (vm["rules"].as<string>().compare("") != 0) {
                string path = vm["rules"].as<string>();
                if (!Utils::exists(path)) {
                    printErrorMsg((string("The rule file ") + path + string(" doe not exists")).c_str());
                    return false;
                }
            }
        } else if (cmd == "lookup") {
            if (!vm.count("text") && !vm.count("number")) {
                printErrorMsg(
                        "Neither the -t nor -n parameters are set. At least one of them must be set.");
                return false;
            }

            if (vm.count("text") && vm.count("number")) {
                printErrorMsg(
                        "Both the -t and -n parameters are set, and this is ambiguous. Please choose either one or the other.");
                return false;
            }
        } else if (cmd == "load") {
            if (!vm.count("input") and !vm.count("comprinput")) {
                printErrorMsg(
                        "The parameter -i (path to the triple files) is not set. Also --comprinput (file with the compressed triples) is not set.");
                return false;
            }

            if (vm.count("comprinput")) {
                string tripleDir = vm["comprinput"].as<string>();
                if (!Utils::exists(tripleDir)) {
                    printErrorMsg(
                            (string("The file ") + tripleDir
                             + string(" does not exist.")).c_str());
                    return false;
                }
                if (!vm.count("comprdict")) {
                    printErrorMsg(
                            "The parameter -comprdict (path to the compressed dict) is not set.");
                    return false;
                }
            } else {
                string tripleDir = vm["input"].as<string>();
                if (!Utils::exists(tripleDir)) {
                    printErrorMsg(
                            (string("The path ") + tripleDir
                             + string(" does not exist.")).c_str());
                    return false;
                }
            }

            if (!vm.count("output")) {
                printErrorMsg(
                        "The parameter -o (path to the kb is not set.");
                return false;
            }
            string kbdir = vm["output"].as<string>();
            if (Utils::exists(kbdir)) {
                printErrorMsg(
                        (string("The path ") + kbdir
                         + string(" already exist. Please remove it or choose another path.")).c_str());
                return false;
            }
            if (vm["maxThreads"].as<int>() < 1) {
                printErrorMsg(
                        "The number of threads to use must be at least 1");
                return false;
            }

            if (vm["readThreads"].as<int>() < 1) {
                printErrorMsg(
                        "The number of threads to use to read the input must be at least 1");
                return false;
            }

        } else if (cmd == "gentq") {
            if (vm["rules"].as<string>().compare("") != 0) {
                string path = vm["rules"].as<string>();
                if (!Utils::exists(path)) {
                    printErrorMsg((string("The rule file ") + path + string(" doe not exists")).c_str());
                    return false;
                }
            }
        } else if (cmd == "mat") {
            string path = vm["rules"].as<string>();
            if (path == "") {
                printErrorMsg(string("You must set up the 'rules' parameter to launch the materialization").c_str());
            }
            if (path != "" && !Utils::exists(path)) {
                printErrorMsg((string("The rule file '") +
                            path + string("' does not exists")).c_str());
                return false;
            }
        }
    }

    return true;
}

bool initParams(int argc, const char** argv, ProgramArgs &vm) {

    ProgramArgs::GroupArgs& query_options = *vm.newGroup("Options for <query>, <queryLiteral> or <mat>");
    query_options.add<string>("q", "query", "",
            "The path of the file with a query. It is REQUIRED with <query> or <queryLiteral>", false);
    query_options.add<string>("","rules", "",
            "Activate reasoning during query answering using the rules defined at this path. It is REQUIRED in case the command is <mat>. Default is '' (disabled).", false);
    query_options.add<bool>("", "rewriteMultihead", false,
            "try to split up rules with multiple heads.", false);
    query_options.add<long>("", "reasoningThreshold", 1000000,
            "This parameter sets a threshold to estimate the reasoning cost of a pattern. This cost can be broadly associated to the cardinality of the pattern. It is used to choose either TopDown or Magic evalution. Default is 1000000 (1M).", false);
    query_options.add<string>("", "reasoningAlgo", "",
            "Determines the reasoning algo (only for <queryLiteral>). Possible values are \"qsqr\", \"magic\", \"auto\".", false);
    query_options.add<string>("", "selectionStrategy", "",
            "Determines the selection strategy (only for <queryLiteral>, when \"auto\" is specified for the reasoningAlgorithm). Possible values are \"cardEst\", ... (to be extended) .", false);
    query_options.add<long>("", "matThreshold", 10000000,
            "In case reasoning is activated, this parameter sets a threshold above which a full materialization is performed before we execute the query. Default is 10000000 (10M).", false);
    query_options.add<bool>("", "printResults", true,
            "Print the answers of a literal query.", false);
    query_options.add<bool>("", "automat", false,
            "Automatically premateralialize some atoms.", false);
    query_options.add<int>("", "timeoutPremat", 1000000,
            "Timeout used during automatic prematerialization (in microseconds). Default is 1000000 (i.e. one second per query)", false);
    query_options.add<string>("", "premat", "",
            "Pre-materialize the atoms in the file passed as argument. Default is '' (disabled).", false);
    query_options.add<bool>("","multithreaded", false,
            "Run multithreaded (currently only supported for <mat>).", false);
    query_options.add<bool>("","restrictedChase", false,
            "Use the restricted chase if there are existential rules.", false);
    query_options.add<int>("", "nthreads", std::max((unsigned int)1, std::thread::hardware_concurrency() / 2),
            string("Set maximum number of threads to use when run in multithreaded mode. Default is " + to_string(std::max((unsigned int)1, std::thread::hardware_concurrency() / 2))).c_str(), false);
    query_options.add<int>("", "interRuleThreads", 0,
            "Set maximum number of threads to use for inter-rule parallelism. Default is 0", false);

    query_options.add<bool>("", "shufflerules", false,
            "shuffle rules randomly instead of using heuristics (only for <mat>, and only when running multithreaded).", false);
    query_options.add<int>("r", "repeatQuery", 0,
            "Repeat the query <arg> times. If the argument is not specified, then the query will not be repeated.", false);
    query_options.add<string>("","storemat_path", "",
            "Directory where to store all results of the materialization. Default is '' (disable).",false);
    query_options.add<string>("","storemat_format", "files",
            "Format in which to dump the materialization. 'files' simply dumps the IDBs in files. 'csv' creates comma-separated files. 'db' creates a new RDF database. Default is 'files'.",false);
    query_options.add<bool>("","explain", false,
            "Explain the query instead of executing it. Default is false.",false);
    query_options.add<bool>("","decompressmat", false,
            "Decompress the results of the materialization when we write it to a file. Default is false.",false);

#ifdef WEBINTERFACE
    query_options.add<bool>("","webinterface", false,
            "Start a web interface to monitor the execution. Default is false.",false);
    query_options.add<int>("","port", 8080, "Port to use for the web interface. Default is 8080",false);
#endif

    query_options.add<bool>("","no-filtering", false, "Disable filter optimization.",false);
    query_options.add<bool>("","no-intersect", false, "Disable intersection optimization.",false);
    query_options.add<string>("","graphfile", "", "Path to store the rule dependency graph",false);

    ProgramArgs::GroupArgs& load_options = *vm.newGroup("Options for <load>");
    load_options.add<string>("i","input", "",
            "Path to the files that contain the compressed triples. This parameter is REQUIRED if already compressed triples/dict are not provided.", false);
    load_options.add<string>("o","output", "",
            "Path to the KB that should be created. This parameter is REQUIRED.", false);
    load_options.add<int>("","maxThreads",
            Utils::getNumberPhysicalCores(),
            "Sets the maximum number of threads to use during the compression. Default is the number of physical cores",false);
    load_options.add<int>("","readThreads", 2,
            "Sets the number of concurrent threads that reads the raw input. Default is '2'",false);
    load_options.add<string>("","comprinput", "",
            "Path to a file that contains a list of compressed triples.",false);
    load_options.add<string>("","comprdict", "",
            "Path to a file that contains the dictionary for the compressed triples.",false);

    ProgramArgs::GroupArgs& lookup_options = *vm.newGroup("Options for <lookup>");
    lookup_options.add<string>("t","text", "",
            "Textual term to search", false);
    lookup_options.add<int>("n","number", 0, "Numeric term to search",false);

    ProgramArgs::GroupArgs& server_options = *vm.newGroup("Options for <server>");
    server_options.add<string>("","webpages", "../webinterface",
            "Path to the webpages relative to where the executable is. Default is ../webinterface", false);

    ProgramArgs::GroupArgs& generateTraining_options = *vm.newGroup("Options for command <gentq>");
    generateTraining_options.add<int>("", "maxTuples", 500, "Number of EDB tuples to consider for training", false);
    generateTraining_options.add<int>("", "depth", 5, "Recursion level of training generation procedure", false);

    ProgramArgs::GroupArgs& cmdline_options = *vm.newGroup("Parameters");
    cmdline_options.add<string>("l","logLevel", "info",
            "Set the log level (accepted values: trace, debug, info, warning, error, fatal). Default is info.", false);

    cmdline_options.add<string>("e", "edb", "default",
            "Path to the edb conf file. Default is 'edb.conf' in the same directory as the exec file.",false);
    cmdline_options.add<int>("","sleep", 0, "sleep <arg> seconds before starting the run. Useful for attaching profiler.",false);

    vm.parse(argc, argv);
    return checkParams(vm, argc, argv);
}

typedef enum QueryType{
    QUERY_TYPE_MIXED = 0,
    QUERY_TYPE_GENERIC = 100,
    QUERY_TYPE_BOOLEAN = 1000
}QueryType;

std::string makeGenericQuery(Program& p, PredId_t predId, uint8_t predCard) {
    std::string query = p.getPredicateName(predId);
    query += "(";
    for (int i = 0; i < predCard; ++i) {
        query += "V" + to_string(i+1);
        if (i != predCard-1) {
            query += ",";
        }
    }
    query += ")";
    return query;
}

std::pair<std::string, int> makeComplexQuery(Program& p, Literal& l, vector<Substitution>& sub, EDBLayer& db) {
    std::string query = p.getPredicateName(l.getPredicate().getId());
    int card = l.getPredicate().getCardinality();
    query += "(";
    QueryType queryType;
    int countConst = 0;
    for (int i = 0; i < card; ++i) {
        std::string canV = "V" + to_string(i+1);
        //FIXME: uint8_t id = p.getIDVar(canV); //I don't know how to convert this line
        uint8_t id = 0;
        bool found = false;
        for (int j = 0; j < sub.size(); ++j) {
            if (sub[j].origin == id) {
                char supportText[MAX_TERM_SIZE];
                db.getDictText(sub[j].destination.getValue(), supportText);
                query += supportText;
                found = true;
                countConst++;
            }
        }
        if (!found) {
            query += canV;
        }
        if (i != card-1) {
            query += ",";
        }
    }
    query += ")";

    if (countConst == card) {
        queryType = QUERY_TYPE_BOOLEAN;
    } else if (countConst == 0) {
        queryType = QUERY_TYPE_GENERIC;
    } else {
        queryType = QUERY_TYPE_MIXED;
    }
    return std::make_pair(query, queryType);
}

template <typename Generic>
std::vector<std::vector<Generic>> powerset(std::vector<Generic>& set) {
    std::vector<std::vector<Generic>> output;
    uint16_t setSize = set.size();
    uint16_t powersetSize = pow(2, setSize) - 1;
    for (int i = 1; i <= powersetSize; ++i) {
        std::vector<Generic> element;
        for (int j = 0; j < setSize; ++j) {
            if (i & (1<<j)) {
                element.push_back(set[j]);
            }
        }
        output.push_back(element);
    }
    return output;
}

PredId_t getMatchingIDB(EDBLayer& db, Program &p, vector<uint64_t>& tuple) {
    //Check this tuple with all rules
    PredId_t idbPredicateId = 65535;
    vector<Rule> rules = p.getAllRules();
    vector<Rule>::iterator it = rules.begin();
    vector<pair<uint8_t, uint64_t>> ruleTuple;
    for (;it != rules.end(); ++it) {
        vector<Literal> body = (*it).getBody();
        if (body.size() > 1) {
            continue;
        }
        uint8_t nConstants = body[0].getNConstants();
        Predicate temp = body[0].getPredicate();
        if (!p.isPredicateIDB(temp.getId())){
            int matched = 0;
            for (int c = 0; c < temp.getCardinality(); ++c) {
                uint8_t tempid = body[0].getTermAtPos(c).getId();
                if(tempid == 0) {
                    uint64_t tempvalue = body[0].getTermAtPos(c).getValue();
                    char supportText[MAX_TERM_SIZE];
                    db.getDictText(tempvalue, supportText);
                    if (tempvalue == tuple[c]) {
                        matched++;
                    }
                }
            }
            if (matched == nConstants) {
                idbPredicateId = (*it).getFirstHead().getPredicate().getId();
                return idbPredicateId;
            }
        }
    }
    return idbPredicateId;
}

std::vector<std::pair<std::string, int>> generateTrainingQueries(EDBLayer &db,
        Program &p,
        std::vector<uint8_t>& vt,
        ProgramArgs &vm
        ) {
    std::unordered_map<string, int> allQueries;

    typedef std::pair<PredId_t, vector<Substitution>> EndpointWithEdge;
    typedef std::unordered_map<uint16_t, std::vector<EndpointWithEdge>> Graph;
    Graph graph;

    std::vector<Rule> rules = p.getAllRules();
    for (int i = 0; i < rules.size(); ++i) {
        Rule ri = rules[i];
        Predicate ph = ri.getFirstHead().getPredicate();
        std::vector<Substitution> sigmaH;
        for (int j = 0; j < ph.getCardinality(); ++j) {
            VTerm dest = ri.getFirstHead().getTuple().get(j);
            sigmaH.push_back(Substitution(vt[j], dest));
        }
        std::vector<Literal> body = ri.getBody();
        for (std::vector<Literal>::const_iterator itr = body.begin(); itr != body.end(); ++itr) {
            Predicate pb = itr->getPredicate();
            std::vector<Substitution> sigmaB;
            for (int j = 0; j < pb.getCardinality(); ++j) {
                VTerm dest = itr->getTuple().get(j);
                sigmaB.push_back(Substitution(vt[j], dest));
            }
            // Calculate sigmaB * sigmaH
            std::vector<Substitution> edge_label = inverse_concat(sigmaB, sigmaH);
            EndpointWithEdge neighbour = std::make_pair(ph.getId(), edge_label);
            graph[pb.getId()].push_back(neighbour);
        }
    }

#if DEBUG
    // Try printing graph
    for (auto it = graph.begin(); it != graph.end(); ++it) {
        uint16_t id = it->first;
        std::cout << p.getPredicateName(id) << " : " << std::endl;
        std::vector<EndpointWithEdge> nei = it->second;
        for (int i = 0; i < nei.size(); ++i) {
            Predicate pred = p.getPredicate(nei[i].first);
            std::vector<Substitution> sub = nei[i].second;
            for (int j = 0; j < sub.size(); ++j){
                std::cout << p.getPredicateName(nei[i].first) << "{" << sub[j].origin << "->"
                    << sub[j].destination.getId() << " , " << sub[j].destination.getValue() << "}" << std::endl;
            }
        }
        std::cout << "=====" << std::endl;
    }
#endif

    // Gather all predicates
    std::vector<PredId_t> ids = p.getAllEDBPredicateIds();
    std::ofstream allPredicatesLog("allPredicatesInQueries.log");
    Dictionary dictVariables;
    for (int i = 0; i < ids.size(); ++i) {
        int neighbours = graph[ids[i]].size();
        LOG(INFOL) << p.getPredicateName(ids[i]) << " is EDB : " << neighbours << "neighbours";
        Predicate edbPred = p.getPredicate(ids[i]);
        int card = edbPred.getCardinality();
        std::string query = makeGenericQuery(p, edbPred.getId(), edbPred.getCardinality());
        Literal literal = p.parseLiteral(query, dictVariables);
        int nVars = literal.getNVars();
        QSQQuery qsqQuery(literal);
        TupleTable *table = new TupleTable(nVars);
        db.query(&qsqQuery, table, NULL, NULL);
        uint64_t nRows = table->getNRows();
        std::vector<std::vector<uint64_t>> output;
        uint64_t maxTuples = vm["maxTuples"].as<unsigned int>();
        /**
         * RP1(A,B) :- TE(A, <studies>, B)
         * RP2(A,B) :- TE(A, <worksFor>, B)
         *
         * Tuple <jon, studies, VU> can match with RP2, which it should not
         *
         * All EDB tuples should be carefully matched with rules
         * */
        PredId_t predId = edbPred.getId();
        uint64_t rowNumber = 0;
        if (maxTuples > nRows) {
            maxTuples = nRows;
        }
        while (rowNumber < maxTuples) {
            std::vector<uint64_t> tuple;
            std::string tupleString("<");
            for (int j = 0; j < nVars; ++j) {
                uint64_t value = table->getPosAtRow(rowNumber, j);
                tuple.push_back(value);
                char supportText[MAX_TERM_SIZE];
                db.getDictText(value, supportText);
                tupleString += supportText;
                tupleString += ",";
            }
            tupleString += ">";
            LOG(INFOL) << "Tuple # " << rowNumber << " : " << tupleString;
            PredId_t idbPredId = getMatchingIDB(db, p, tuple);
            if (65535 == idbPredId) {
                rowNumber++;
                continue;
            }
            std::string predName = p.getPredicateName(idbPredId);

            LOG(INFOL) << tupleString << " ==> " << predName << " : " << +idbPredId;
            vector<Substitution> subs;
            for (int k = 0; k < card; ++k) {
                subs.push_back(Substitution(vt[k], VTerm(0, tuple[k])));
            }
            // Find powerset of subs here
            std::vector<std::vector<Substitution>> options =  powerset<Substitution>(subs);
            unsigned int seed = (unsigned int) ((clock() ^ 413711) % 105503);
            srand(seed);
            for (int l = 0; l < options.size(); ++l) {
                int depth = vm["depth"].as<unsigned int>();
                vector<Substitution> sigma = options[l];
                PredId_t predId = edbPred.getId();
                int n = 1;
                while (n != depth+1) {
                    uint32_t nNeighbours = graph[predId].size();
                    if (0 == nNeighbours) {
                        break;
                    }
                    uint32_t randomNeighbour;
                    if (1 == n) {
                        int index = 0;
                        bool found = false;
                        for (auto it = graph[predId].begin(); it != graph[predId].end(); ++it,++index) {
                            if (it->first == idbPredId) {
                                randomNeighbour = index;
                                found = true;
                                break;
                            }
                        }
                        assert(found == true);
                    } else {
                        randomNeighbour = rand() % nNeighbours;
                    }
                    std::vector<Substitution>sigmaN = graph[predId][randomNeighbour].second;
                    std::vector<Substitution> result = concat(sigmaN, sigma);
                    PredId_t qId  = graph[predId][randomNeighbour].first;
                    uint8_t qCard = p.getPredicate(graph[predId][randomNeighbour].first).getCardinality();
                    std::string qQuery = makeGenericQuery(p, qId, qCard);
                    Literal qLiteral = p.parseLiteral(qQuery, dictVariables);
                    allPredicatesLog << p.getPredicateName(qId) << std::endl;
                    std::pair<string, int> finalQueryResult = makeComplexQuery(p, qLiteral, result, db);
                    std::string qFinalQuery = finalQueryResult.first;
                    int type = finalQueryResult.second + ((n > 4) ? 4 : n);
                    if (allQueries.find(qFinalQuery) == allQueries.end()) {
                        allQueries.insert(std::make_pair(qFinalQuery, type));
                    }

                    predId = qId;
                    sigma = result;
                    n++;
                } // while the depth of exploration is reached
            } // for each partial substitution
            rowNumber++;
        }
    } // all EDB predicate ids
    allPredicatesLog.close();
    std::vector<std::pair<std::string,int>> queries;
    for (std::unordered_map<std::string,int>::iterator it = allQueries.begin(); it !=  allQueries.end(); ++it) {
        queries.push_back(std::make_pair(it->first, it->second));
        LOG(INFOL) << "Query: " << it->first << " type : " << it->second ;
    }
    return queries;
}

void lookup(EDBLayer &layer, ProgramArgs &vm) {
    if (vm.count("text")) {
        uint64_t value;
        string textTerm = vm["text"].as<string>();
        if (!layer.getDictNumber((char*) textTerm.c_str(), textTerm.size(), value)) {
            cout << "Term " << textTerm << " not found" << endl;
        } else {
            cout << value << endl;
        }
    } else {
        uint64_t key = vm["number"].as<long>();
        char supportText[MAX_TERM_SIZE];
        if (!layer.getDictText(key, supportText)) {
            cout << "Term " << key << " not found" << endl;
        } else {
            cout << supportText << endl;
        }
    }
}

string flattenAllArgs(int argc,
        const char** argv) {
    string args = "";
    for (int i = 1; i < argc; ++i) {
        args += " " + string(argv[i]);
    }
    return args;
}

void writeRuleDependencyGraph(EDBLayer &db, string pathRules, string filegraph) {
    LOG(INFOL) << " Write the graph file to " << filegraph;
    Program p(db.getNTerms(), &db);
    p.readFromFile(pathRules, false);
    std::shared_ptr<SemiNaiver> sn = Reasoner::getSemiNaiver(db,
            &p, true, true, false, false, 1, 1, false);

    std::vector<int> nodes;
    std::vector<std::pair<int, int>> edges;
    sn->createGraphRuleDependency(nodes, edges);

    //Write down the details of the graph on a file
    ofstream fout(filegraph);
    fout << "#nodes" << endl;
    for (auto el : nodes)
        fout << to_string(el) << endl;
    fout << "#edges" << endl;
    for (auto el : edges)
        fout << el.first << "\t" << el.second << endl;
    fout.close();
}
#ifdef WEBINTERFACE
void startServer(int argc,
        const char** argv,
        string pathExec,
        ProgramArgs &vm) {
    std::unique_ptr<WebInterface> webint;
    int port = vm["port"].as<int>();
    std::string webinterface = vm["webpages"].as<string>();
    webint = std::unique_ptr<WebInterface>(
            new WebInterface(vm, NULL, pathExec + "/" + webinterface,
                flattenAllArgs(argc, argv),
                vm["edb"].as<string>()));
    webint->start(port);
    LOG(INFOL) << "Server is launched at 0.0.0.0:" << to_string(port);
    webint->join();
}
#endif

void launchFullMat(int argc,
        const char** argv,
        string pathExec,
        EDBLayer &db,
        ProgramArgs &vm,
        std::string pathRules) {
    //Load a program with all the rules
    Program p(db.getNTerms(), &db);
    p.readFromFile(pathRules,vm["rewriteMultihead"].as<bool>());

    //Existential check
    if (p.areExistentialRules()) {
        LOG(INFOL) << "The program might not terminate due to existential rules ...";
    }

    //Set up the ruleset and perform the pre-materialization if necessary
    {
        if (!vm["automat"].empty()) {
            //Automatic prematerialization
            std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
            Materialization mat;
            mat.guessLiteralsFromRules(p, db);
            mat.getAndStorePrematerialization(db, p, true,
                    vm["timeoutPremat"].as<int>());
            std::chrono::duration<double> sec = std::chrono::system_clock::now()
                - start;
            LOG(INFOL) << "Runtime pre-materialization = " <<
                sec.count() * 1000 << " milliseconds";
        } else if (vm["premat"].as<string>() != "") {
            std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
            Materialization mat;
            mat.loadLiteralsFromFile(p, vm["premat"].as<string>());
            mat.getAndStorePrematerialization(db, p, false, ~0l);
            std::chrono::duration<double> sec = std::chrono::system_clock::now()
                - start;
            LOG(INFOL) << "Runtime pre-materialization = " <<
                sec.count() * 1000 << " milliseconds";
        }

        int nthreads = vm["nthreads"].as<int>();
        if (vm["multithreaded"].empty()) {
            nthreads = -1;
        }
        int interRuleThreads = vm["interRuleThreads"].as<int>();
        if (vm["multithreaded"].empty()) {
            interRuleThreads = 0;
        }

        //Prepare the materialization
        std::shared_ptr<SemiNaiver> sn = Reasoner::getSemiNaiver(db,
                &p, vm["no-intersect"].empty(),
                vm["no-filtering"].empty(),
                !vm["multithreaded"].empty(),
                vm["restrictedChase"].as<bool>(),
                nthreads,
                interRuleThreads,
                ! vm["shufflerules"].empty());

#ifdef WEBINTERFACE
        //Start the web interface if requested
        std::unique_ptr<WebInterface> webint;
        if (vm["webinterface"].as<bool>()) {
            webint = std::unique_ptr<WebInterface>(
                    new WebInterface(vm, sn, pathExec + "/webinterface",
                        flattenAllArgs(argc, argv),
                        vm["edb"].as<string>()));
            int port = vm["port"].as<int>();
            webint->start(port);
        }
#endif

        LOG(INFOL) << "Starting full materialization";
        std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
        sn->run();
        std::chrono::duration<double> sec = std::chrono::system_clock::now() - start;
        LOG(INFOL) << "Runtime materialization = " << sec.count() * 1000 << " milliseconds";
        sn->printCountAllIDBs("");

        if (vm["storemat_path"].as<string>() != "") {
            std::chrono::system_clock::time_point start = std::chrono::system_clock::now();

            Exporter exp(sn);

            string storemat_format = vm["storemat_format"].as<string>();

            if (storemat_format == "files" || storemat_format == "csv") {
                sn->storeOnFiles(vm["storemat_path"].as<string>(),
                        vm["decompressmat"].as<bool>(), 0, storemat_format == "csv");
            } else if (storemat_format == "db") {
                //I will store the details on a Trident index
                exp.generateTridentDiffIndex(vm["storemat_path"].as<string>());
            } else if (storemat_format == "nt") {
                exp.generateNTTriples(vm["storemat_path"].as<string>(), vm["decompressmat"].as<bool>());
            } else {
                LOG(ERRORL) << "Option 'storemat_format' not recognized";
                throw 10;
            }

            std::chrono::duration<double> sec = std::chrono::system_clock::now() - start;
            LOG(INFOL) << "Time to index and store the materialization on disk = " << sec.count() << " seconds";
        }
#ifdef WEBINTERFACE
        if (webint) {
            //Sleep for max 1 second, to allow the fetching of the last statistics
            LOG(INFOL) << "Sleeping for one second to allow the web interface to get the last stats ...";
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            LOG(INFOL) << "Done.";
            webint->stop();
        }
#endif
    }
}

void execSPARQLQuery(EDBLayer &edb, ProgramArgs &vm) {
    //Parse the rules and create a program
    Program p(edb.getNTerms(), &edb);
    string pathRules = vm["rules"].as<string>();
    if (pathRules != "") {
        p.readFromFile(pathRules,vm["rewriteMultihead"].as<bool>());
        p.sortRulesByIDBPredicates();
    }

    //Set up the ruleset and perform the pre-materialization if necessary
    if (pathRules != "") {
        if (!vm["automat"].empty()) {
            //Automatic prematerialization
            std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
            Materialization *mat = new Materialization();
            mat->guessLiteralsFromRules(p, edb);
            mat->getAndStorePrematerialization(edb, p, true,
                    vm["timeoutPremat"].as<int>());
            delete mat;
            std::chrono::duration<double> sec = std::chrono::system_clock::now()
                - start;
            LOG(INFOL) << "Runtime pre-materialization = " <<
                sec.count() * 1000 << " milliseconds";
        } else if (vm["premat"].as<string>() != "") {
            std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
            Materialization *mat = new Materialization();
            mat->loadLiteralsFromFile(p, vm["premat"].as<string>());
            mat->getAndStorePrematerialization(edb, p, false, ~0l);
            p.sortRulesByIDBPredicates();
            delete mat;
            std::chrono::duration<double> sec = std::chrono::system_clock::now()
                - start;
            LOG(INFOL) << "Runtime pre-materialization = " <<
                sec.count() * 1000 << " milliseconds";
        }
    }

    DBLayer *db = NULL;
    if (pathRules == "") {
        PredId_t p = edb.getFirstEDBPredicate();
        string typedb = edb.getTypeEDBPredicate(p);
        if (typedb == "Trident") {
            auto edbTable = edb.getEDBTable(p);
            KB *kb = ((TridentTable*)edbTable.get())->getKB();
            TridentLayer *tridentlayer = new TridentLayer(*kb);
            tridentlayer->disableBifocalSampling();
            db = tridentlayer;
        }
    }
    if (db == NULL) {
        if (pathRules == "") {
            // Use default rule
            p.readFromFile(pathRules,vm["rewriteMultihead"].as<bool>());
            p.sortRulesByIDBPredicates();
        }
        db = new VLogLayer(edb, p, vm["reasoningThreshold"].as<long>(), "TI", "TE");
    }
    string queryFileName = vm["query"].as<string>();
    // Parse the query
    std::fstream inFile;
    inFile.open(queryFileName);//open the input file
    std::stringstream strStream;
    strStream << inFile.rdbuf();//read the file

    //TODO
    //WebInterface::execSPARQLQuery(strStream.str(), vm["explain"].as<bool>(),
    //        edb.getNTerms(), *db, true, false, NULL, NULL,
    //        NULL);

    delete db;

    /*QueryDict queryDict(edb.getNTerms());
      QueryGraph queryGraph;
      bool parsingOk;

      SPARQLLexer lexer(strStream.str());
      SPARQLParser parser(lexer);
      std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
      parseQuery(parsingOk, parser, queryGraph, queryDict, db);
      if (!parsingOk) {
      std::chrono::duration<double> duration = std::chrono::system_clock::now() - start;
      LOG(INFOL) << "Runtime query: 0ms.";
      LOG(INFOL) << "Runtime total: " << duration.count() * 1000 << "ms.";
      LOG(INFOL) << "# rows = 0";
      return;
      }

    // Run the optimizer
    PlanGen *plangen = new PlanGen();
    Plan* plan = plangen->translate(db, queryGraph);
    // delete plangen;  Commented out, because this also deletes all plans!
    // In particular, it corrupts the current plan.
    // --Ceriel
    if (!plan) {
    cerr << "internal error plan generation failed" << endl;
    delete plangen;
    return;
    }
    bool explain = vm["explain"].as<bool>();
    if (explain)
    plan->print(0);

    // Build a physical plan
    Runtime runtime(db, NULL, &queryDict);
    Operator* operatorTree = CodeGen().translate(runtime, queryGraph, plan, false);

    // Execute it
    if (explain) {
    DebugPlanPrinter out(runtime, false);
    operatorTree->print(out);
    delete operatorTree;
    } else {
#if DEBUG
DebugPlanPrinter out(runtime, false);
operatorTree->print(out);
#endif
std::chrono::system_clock::time_point startQ = std::chrono::system_clock::now();
if (operatorTree->first()) {
while (operatorTree->next());
}
std::chrono::duration<double> durationQ = std::chrono::system_clock::now() - startQ;
std::chrono::duration<double> duration = std::chrono::system_clock::now() - start;
LOG(INFOL) << "Runtime query: " << durationQ.count() * 1000 << "ms.";
LOG(INFOL) << "Runtime total: " << duration.count() * 1000 << "ms.";
ResultsPrinter *p = (ResultsPrinter*) operatorTree;
long nElements = p->getPrintedRows();
LOG(INFOL) << "# rows = " << nElements;

delete plangen;
delete operatorTree;

int times = vm["repeatQuery"].as<int>();
if (times > 0) {
    // Redirect output
    ofstream file("/dev/null");
    streambuf* strm_buffer = cout.rdbuf();
    cout.rdbuf(file.rdbuf());

    for (int i = 0; i < times; i++) {
    PlanGen *plangen = new PlanGen();
    Plan* plan = plangen->translate(db, queryGraph);
    Runtime runtime(db, NULL, &queryDict);
    operatorTree = CodeGen().translate(runtime, queryGraph, plan, false);
    startQ = std::chrono::system_clock::now();
    if (operatorTree->first()) {
        while (operatorTree->next());
    }
    durationQ += std::chrono::system_clock::now() - startQ;
    p = (ResultsPrinter*) operatorTree;
    long n1 = p->getPrintedRows();
    if (n1 != nElements) {
        LOG(ERRORL) << "Number of records (" << n1 << ") is not the same. This should not happen...";
    }
    delete plangen;
    delete operatorTree;
}
LOG(INFOL) << "Repeated query runtime = " << (durationQ.count() / times) * 1000
<< " milliseconds";
//Restore stdout
cout.rdbuf(strm_buffer);
}
}*/
}

string selectStrategy(EDBLayer &edb, Program &p, Literal &literal, Reasoner &reasoner, ProgramArgs &vm) {
    string strategy = vm["selectionStrategy"].as<string>();
    if (strategy == "" || strategy == "cardEst") {
        // Use the original cardinality estimation strategy
        ReasoningMode mode = reasoner.chooseMostEfficientAlgo(literal, edb, p, NULL, NULL);
        return mode == TOPDOWN ? "qsqr" : "magic";
    }
    // Add strategies here ...
    LOG(ERRORL) << "Unrecognized selection strategy: " << strategy;
    throw 10;
}

void runLiteralQuery(EDBLayer &edb, Program &p, Literal &literal, Reasoner &reasoner, ProgramArgs &vm) {

    std::chrono::system_clock::time_point startQ1 = std::chrono::system_clock::now();

    string algo = vm["reasoningAlgo"].as<string>();
    int times = vm["repeatQuery"].as<int>();
    bool printResults = vm["printResults"].as<bool>();

    int nVars = literal.getNVars();
    bool onlyVars = nVars > 0;

    if (literal.getPredicate().getType() == EDB) {
        if (algo != "edb") {
            LOG(INFOL) << "Overriding strategy, setting it to edb";
            algo = "edb";
        }
    }

    if (algo == "auto" || algo == "") {
        algo = selectStrategy(edb, p, literal, reasoner, vm);
        LOG(INFOL) << "Selection strategy determined that we go for " << algo;
    }

    TupleIterator *iter;

    if (algo == "edb") {
        iter = reasoner.getEDBIterator(literal, NULL, NULL, edb, onlyVars, NULL);
    } else if (algo == "magic") {
        iter = reasoner.getMagicIterator(literal, NULL, NULL, edb, p, onlyVars, NULL);
    } else if (algo == "qsqr") {
        iter = reasoner.getTopDownIterator(literal, NULL, NULL, edb, p, onlyVars, NULL);
    } else if (algo == "mat") {
        iter = reasoner.getMaterializationIterator(literal, NULL, NULL, edb, p, onlyVars, NULL);
    } else {
        LOG(ERRORL) << "Unrecognized reasoning algorithm: " << algo;
        throw 10;
    }
    long count = 0;
    int sz = iter->getTupleSize();
    if (nVars == 0) {
        cout << (iter->hasNext() ? "TRUE" : "FALSE") << endl;
        count = (iter->hasNext() ? 1 : 0);
    } else {
        while (iter->hasNext()) {
            iter->next();
            count++;
            if (printResults) {
                for (int i = 0; i < sz; i++) {
                    char supportText[MAX_TERM_SIZE];
                    uint64_t value = iter->getElementAt(i);
                    if (i != 0) {
                        cout << " ";
                    }
                    if (!edb.getDictText(value, supportText)) {
                        cerr << "Term " << value << " not found" << endl;
                        cout << value;
                    } else {
                        cout << supportText;
                    }
                }
                cout << endl;
            }
        }
    }
    std::chrono::duration<double> durationQ1 = std::chrono::system_clock::now() - startQ1;
    LOG(INFOL) << "Algo = " << algo << ", cold query runtime = " << (durationQ1.count() * 1000) << " msec, #rows = " << count;

    delete iter;
    if (times > 0) {
        // Redirect output
        ofstream file("/dev/null");
        streambuf* strm_buffer = cout.rdbuf();
        cout.rdbuf(file.rdbuf());
        std::chrono::system_clock::time_point startQ = std::chrono::system_clock::now();
        for (int j = 0; j < times; j++) {
            TupleIterator *iter = reasoner.getIterator(literal, NULL, NULL, edb, p, true, NULL);
            int sz = iter->getTupleSize();
            while (iter->hasNext()) {
                iter->next();
                if (printResults) {
                    for (int i = 0; i < sz; i++) {
                        char supportText[MAX_TERM_SIZE];
                        uint64_t value = iter->getElementAt(i);
                        if (i != 0) {
                            cout << ", ";
                        }
                        if (!edb.getDictText(value, supportText)) {
                            cout << value;
                        } else {
                            cout << supportText;
                        }
                    }
                    cout << endl;
                }
            }
        }
        std::chrono::duration<double> durationQ = std::chrono::system_clock::now() - startQ;
        //Restore stdout
        cout.rdbuf(strm_buffer);
        LOG(INFOL) << "Algo = " << algo << ", average warm query runtime = " << (durationQ.count() / times) * 1000 << " milliseconds";
    }
}

void execLiteralQuery(EDBLayer &edb, ProgramArgs &vm) {
    //Parse the rules and create a program
    Program p(edb.getNTerms(), &edb);
    string pathRules = vm["rules"].as<string>();
    if (pathRules != "") {
        p.readFromFile(pathRules,vm["rewriteMultihead"].as<bool>());
        p.sortRulesByIDBPredicates();
    }

    //Set up the ruleset and perform the pre-materialization if necessary
    if (pathRules != "") {
        if (!vm["automat"].empty()) {
            //Automatic prematerialization
            std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
            Materialization *mat = new Materialization();
            mat->guessLiteralsFromRules(p, edb);
            mat->getAndStorePrematerialization(edb, p, true,
                    vm["timeoutPremat"].as<int>());
            delete mat;
            std::chrono::duration<double> sec = std::chrono::system_clock::now()
                - start;
            LOG(INFOL) << "Runtime pre-materialization = " <<
                sec.count() * 1000 << " milliseconds";
        } else if (vm["premat"].as<string>() != "") {
            std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
            Materialization *mat = new Materialization();
            mat->loadLiteralsFromFile(p, vm["premat"].as<string>());
            mat->getAndStorePrematerialization(edb, p, false, ~0l);
            p.sortRulesByIDBPredicates();
            delete mat;
            std::chrono::duration<double> sec = std::chrono::system_clock::now()
                - start;
            LOG(INFOL) << "Runtime pre-materialization = " <<
                sec.count() * 1000 << " milliseconds";
        }
    }

    string query;
    string queryFileName = vm["query"].as<string>();
    if (Utils::exists(queryFileName)) {
        // Parse the query
        std::fstream inFile;
        inFile.open(queryFileName);//open the input file
        std::getline(inFile, query);
        inFile.close();
    } else {
        query = queryFileName;
    }
    Dictionary dictVariables;
    Literal literal = p.parseLiteral(query, dictVariables);
    Reasoner reasoner(vm["reasoningThreshold"].as<long>());
    runLiteralQuery(edb, p, literal, reasoner, vm);
}

int main(int argc, const char** argv) {

    //Init params
    ProgramArgs vm;
    if (!initParams(argc, argv, vm)) {
        return EXIT_FAILURE;
    }
    string full_path = Utils::getFullPathExec();
    //Set logging level
    string ll = vm["logLevel"].as<string>();
    if (ll == "debug") {
        Logger::setMinLevel(DEBUGL);
    } else if (ll == "info") {
        Logger::setMinLevel(INFOL);
    } else if (ll == "warning") {
        Logger::setMinLevel(WARNL);
    } else if (ll == "error") {
        Logger::setMinLevel(ERRORL);
    }

    string cmd = string(argv[1]);

    //Get the path to the EDB layer
    string edbFile = vm["edb"].as<string>();
    if (edbFile == "default") {
        //Get current directory
        string execFile = string(argv[0]);
        string dirExecFile = Utils::parentDir(execFile);
        edbFile = dirExecFile + string("/edb.conf");
    }

    if (cmd != "load" && !Utils::exists(edbFile)) {
        printErrorMsg(string("I could not find the EDB conf file " + edbFile).c_str());
        return EXIT_FAILURE;
    }

    //set up parallelism in the TBB library
    size_t parallelism = vm["nthreads"].as<int>();
    if (parallelism <= 1) {
        parallelism = 2;    // Otherwise tbb aborts.
        // Actual parallelism will be controlled elsewhere.
    }
    // Allow for older tbb versions: don't use global_control.
    // tbb::global_control c(tbb::global_control::max_allowed_parallelism, parallelism);
    ParallelTasks::setNThreads(parallelism);
    //tbb::task_scheduler_init init(parallelism);

    // For profiling:
    int seconds = vm["sleep"].as<int>();
    if (seconds > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(seconds * 1000));
    }

    std::chrono::system_clock::time_point start = std::chrono::system_clock::now();

    LOG(DEBUGL) << "sizeof(EDBLayer) = " << sizeof(EDBLayer);

    if (cmd == "query" || cmd == "queryLiteral") {
        EDBConf conf(edbFile);
        EDBLayer *layer = new EDBLayer(conf, false);

        //Execute the query
        if (cmd == "query") {
            execSPARQLQuery(*layer, vm);
        } else {
            execLiteralQuery(*layer, vm);
        }
        delete layer;
    } else if (cmd == "lookup") {
        EDBConf conf(edbFile);
        EDBLayer *layer = new EDBLayer(conf, false);
        lookup(*layer, vm);
        delete layer;
    } else if (cmd == "mat") {
        EDBConf conf(edbFile);
        EDBLayer *layer = new EDBLayer(conf, ! vm["multithreaded"].empty());
        // EDBLayer layer(conf, false);
        launchFullMat(argc, argv, full_path, *layer, vm,
                vm["rules"].as<string>());
        delete layer;
    } else if (cmd == "rulesgraph") {
        EDBConf conf(edbFile);
        EDBLayer *layer = new EDBLayer(conf, false);
        writeRuleDependencyGraph(*layer, vm["rules"].as<string>(),
                vm["graphfile"].as<string>());
        delete layer;
    } else if (cmd == "load") {
        Loader *loader = new Loader();
        bool onlyCompress = false;
        int sampleMethod = PARSE_COUNTMIN;
        string dictMethod = DICT_HEURISTICS;
        int popArg = 1000;
        int nindices = 6;
        bool aggrIndices = false;
        int fixedStrat = StorageStrat::FIXEDSTRAT5;
        bool enableFixedStrat = true;
        bool storePlainList = false;
        double sampleRate = 0.01;
        bool sample = true;
        int ndicts = 1;
        bool canSkipTables = false;
        int thresholdSkipTable = 0;
        string popMethod = "hash";
        if (vm.count("comprinput")) {
            string comprinput = vm["comprinput"].as<string>();
            string comprdict = vm["comprdict"].as<string>();
            LOG(INFOL) << "Creating the KB from " << comprinput << "/" << comprdict;

            ParamsLoad p;
            p.inputformat = "rdf";
            p.onlyCompress = onlyCompress;
            p.inputCompressed = true;
            p.triplesInputDir =  vm["comprinput"].as<string>();
            p.dictDir = vm["comprdict"].as<string>();
            p.tmpDir = vm["output"].as<string>();
            p.kbDir = vm["output"].as<string>();
            p.dictMethod = dictMethod;
            p.sampleMethod = sampleMethod;
            p.sampleArg = popArg;
            p.parallelThreads = vm["maxThreads"].as<int>();
            p.maxReadingThreads = vm["readThreads"].as<int>();
            p.dictionaries = ndicts;
            p.nindices = nindices;
            p.createIndicesInBlocks = false;    // true not working???
            p.aggrIndices = aggrIndices;
            p.canSkipTables = canSkipTables;
            p.enableFixedStrat = enableFixedStrat;
            p.fixedStrat = fixedStrat;
            p.storePlainList = storePlainList;
            p.sample = sample;
            p.sampleRate = sampleRate;
            p.thresholdSkipTable = thresholdSkipTable;
            p.remoteLocation = "";
            p.limitSpace = 0;
            p.graphTransformation = "";
            p.timeoutStats = -1;
            p.storeDicts = true;
            p.relsOwnIDs = false;
            p.flatTree = false;

            loader->load(p);

        } else {
            LOG(INFOL) << "Creating the KB from " << vm["input"].as<string>();


            ParamsLoad p;
            p.inputformat = "rdf";
            p.onlyCompress = false;
            p.inputCompressed = false;
            p.triplesInputDir =  vm["input"].as<string>();
            p.dictDir = "";
            p.tmpDir = vm["output"].as<string>();
            p.kbDir = vm["output"].as<string>();
            p.dictMethod = dictMethod;
            p.sampleMethod = sampleMethod;
            p.sampleArg = popArg;
            p.parallelThreads = vm["maxThreads"].as<int>();
            p.maxReadingThreads = vm["readThreads"].as<int>();
            p.dictionaries = ndicts;
            p.nindices = nindices;
            p.createIndicesInBlocks = false;    // true not working???
            p.aggrIndices = aggrIndices;
            p.canSkipTables = canSkipTables;
            p.enableFixedStrat = enableFixedStrat;
            p.fixedStrat = fixedStrat;
            p.storePlainList = storePlainList;
            p.sample = sample;
            p.sampleRate = sampleRate;
            p.thresholdSkipTable = thresholdSkipTable;
            p.remoteLocation = "";
            p.limitSpace = 0;
            p.graphTransformation = "";
            p.timeoutStats = -1;
            p.storeDicts = true;
            p.relsOwnIDs = false;
            p.flatTree = false;

            loader->load(p);
        }
        delete loader;
    } else if (cmd == "gentq") {
        EDBConf conf(edbFile);
        string rulesFile = vm["rules"].as<string>();
        EDBLayer *layer = new EDBLayer(conf, false);
        Program p(layer->getNTerms(), layer);
        //uint8_t vt1 = (uint8_t) p.getIDVar("V1");
        //uint8_t vt2 = (uint8_t) p.getIDVar("V2");
        //uint8_t vt3 = (uint8_t) p.getIDVar("V3");
        //uint8_t vt4 = (uint8_t) p.getIDVar("V4");
        uint8_t vt1 = 0;
        uint8_t vt2 = 1;
        uint8_t vt3 = 2;
        uint8_t vt4 = 3;
        std::vector<uint8_t> vt;
        vt.push_back(vt1);
        vt.push_back(vt2);
        vt.push_back(vt3);
        vt.push_back(vt4);
        p.readFromFile(rulesFile);
        std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
        std::vector<std::pair<std::string,int>> trainingQueries = generateTrainingQueries(*layer, p, vt, vm);
        std::chrono::duration<double> sec = std::chrono::system_clock::now()- start;
        int nQueries = trainingQueries.size();
        LOG(INFOL) << nQueries << " queries generated in " << sec.count() << " seconds";
        std::string trainingFileName = extractFileName(rulesFile);
        trainingFileName += "-training.log";
        std::ofstream logFile(trainingFileName);
        for (auto it = trainingQueries.begin(); it != trainingQueries.end(); ++it) {
            logFile << it->first <<":"<<it->second << std::endl;
        }
        if (logFile.fail()) {
            LOG(INFOL) << "Error writing to the log file";
        }
        logFile.close();
    } else if (cmd == "server") {
#ifdef WEBINTERFACE
        startServer(argc, argv, full_path, vm);
#else
        cerr << "The program is not compiled with the web interface activated." << endl;
        return 1;
#endif
    }
    std::chrono::duration<double> sec = std::chrono::system_clock::now() - start;
    LOG(INFOL) << "Runtime = " << sec.count() * 1000 << " milliseconds";

    //Print other stats
    LOG(INFOL) << "Max memory used: " << Utils::get_max_mem() << " MB";
    return EXIT_SUCCESS;
}

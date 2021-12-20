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
}
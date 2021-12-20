#include <vlog/reliances/experiments.h>

void experimentCoreStratified(const std::string &rulePath, bool pieceDecomposition)
{
    std::cout << "Launches coreStratified experiment with parameters " << '\n';
    std::cout << "\t" << rulePath << '\n';
    std::cout << "\t" << ((pieceDecomposition) ? "true" : "false") << std::endl;
}
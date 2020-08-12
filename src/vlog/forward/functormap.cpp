#include <vlog/functormap.h>

void FunctorMap::init(Program *program)
{
    const std::unordered_map<uint32_t, Functor_t> &mapFunctors = program->
        getFunctors();
    for(auto &m : mapFunctors) {
        uint64_t startCounter = RULE_SHIFT(program->getNRules() + m.second.fId);
        std::vector<Var_t> argvars;
        for(size_t i = 0; i < m.second.nargs; ++i) {
            argvars.push_back(i);
        }
        map.insert(std::make_pair(m.first,
                    ChaseMgmt::Rows(startCounter,
                        m.second.nargs,
                        argvars,
                        TypeChase::SKOLEM_CHASE)));
    }
}

uint64_t FunctorMap::getID(uint32_t fId, uint64_t *row)
{
    if (!map.count(fId)) {
        LOG(ERRORL) << "Functor " << fId << " not valid";
        throw 10;
    }
    auto v = map.find(fId);
    uint64_t out;
    if (!v->second.existingRow(row, out)) {
        out = v->second.addRow(row);
    }
    return out;
}

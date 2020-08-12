#ifndef _FUNCTOR_MAP_H
#define _FUNCTOR_MAP_H

#include <vlog/chasemgmt.h>
#include <vlog/concepts.h>

class FunctorMap
{
    private:
        std::map<uint32_t, ChaseMgmt::Rows> map;

    public:
        void init(Program *program);

        uint64_t getID(uint32_t fId, uint64_t *row);
};

#endif

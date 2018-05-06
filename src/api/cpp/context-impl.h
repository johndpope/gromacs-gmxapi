//
// Created by Eric Irrgang on 5/5/18.
//

#ifndef GMXAPI_CONTEXT_IMPL_H
#define GMXAPI_CONTEXT_IMPL_H

#include "config.h"

namespace gmxapi
{

enum class MpiType : unsigned int {
        none = 0,
        mpi = 1,
        tmpi = 2
};

constexpr MpiType gmxMpiType()
{
#if GMX_LIB_MPI
    return MpiType::mpi;
#elif GMX_THREAD_MPI
    return MpiType::tmpi;
#else
    return MpiType::none;
#endif
}

constexpr bool gmxHasMpi()
{
    return static_cast<unsigned int>(gmxMpiType()) > 0;
}

} // end namespace gmxapi

#endif //GMXAPI_CONTEXT_IMPL_H

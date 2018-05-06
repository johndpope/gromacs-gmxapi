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

/*!
 * \brief Check whether GROMACS is compiled with MPI or thread-MPI
 *
 * With MPI or thread-MPI, rank-type multiprocessing is performend, such as for domain decomposition.
 * Notably, this function does not distinguish between thread-level and process-level parallelism.
 * Equivalent to the GMX_MPI preprocessor macro, which client code may not have access to.
 *
 * If this function can return true, then libgromacs contains MPI symbols from the gromacs/utility/gmxmpi.h header
 * and client code should be using gromacs/utility/basenetwork.h.
 *
 * \return true if compiled with GMX_MPI
 */
constexpr bool gmxHasRankParallelism()
{
    return static_cast<unsigned int>(gmxMpiType()) > 0;
}

/*!
 * \brief Test for a build with a standard Message Passing Interface implementation.
 *
 * The gromacs/utility/gmxmpi.h header makes definitions that will conflict with mpi.h, which can
 * be confusing for client code that also attempts to use MPI. This function provides disambiguation.
 *
 * Can be used for runtime branching logic related to sharing an MPI context between libgromacs and client code.
 *
 * \return true if GROMACS is compiled with support for MPI communications.
 */
constexpr bool gmxHasMPI()
{
    return gmxMpiType() == MpiType::mpi;
}

/*!
 * \brief Test for a build with a GROMACS thread-MPI MPI emulation.
 *
 * The gromacs/utility/gmxmpi.h header makes definitions that will conflict with mpi.h, which can
 * be confusing for client code that also attempts to use MPI. This function provides disambiguation.
 *
 * \return true if GROMACS is compiled with thread-MPI instead of MPI.
 */
constexpr bool gmxHasThreadMPI()
{
    return gmxMpiType() == MpiType::tmpi;
}

} // end namespace gmxapi

#endif //GMXAPI_CONTEXT_IMPL_H

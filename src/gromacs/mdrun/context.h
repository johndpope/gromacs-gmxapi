//
// Created by Eric Irrgang on 5/16/18.
//

#ifndef GROMACS_CONTEXT_H
#define GROMACS_CONTEXT_H

// Ugh... avoiding this header dependency is as ugly as not...
#include "gromacs/mdlib/simulationsignal.h"

namespace gmx
{

class Mdrunner;

namespace md
{

/*!
 * \brief Encapsulate some runtime context for sharing in the mdrun call stack.
 *
 * In the future, this functionality can be moved to an updated ProgramContext and
 * the Context should only provide high-level or external information directly. Its
 * primary purpose will be to register and hold factory function pointers with which
 * callers can get handles to the resources they need.
 *
 * Since those modules and resources don't exist yet, we're providing a few directly.
 *
 * An actual API Context should unambiguously point to the same shared resources
 * and configuration throughout the call stack. It should be owned by the calling
 * code and shared down into the library. For the most flexibility, the interface
 * should be a handle that can be safely passed across API boundaries and should
 * be resistant to misuse (sensible copy semantics and RAII state). Implementation
 * details can be worked out for future versions.
 *
 * For this version, a copy of the Context refers to the same resources as the
 * original and is guaranteed to continue to do so because the resources
 * represented are invariant for the life of the Context. However,
 *
 * \warning
 * This implementation does **not** own the resources it proxies and cannot
 * extend their life time. Refer to gmxapi milestone 5, described at https://redmine.gromacs.org/issues/2587
 */
class Context
{
    public:
        /*!
         * \brief Construct with the runner's one resource: a pointer to the owning runner.
         *
         * The Context should be owned by a runner and the Context lifetime should be entirely
         * within the Runner's life.
         *
         * \param runner non-owning pointer to the runner that owns the Context object.
         */
        explicit Context(const Mdrunner &runner);

        /*!
         * \brief Get a reference to the current array of signal flags.
         *
         * There is no guarantee that the flags have been initialized yet.
         *
         * \return pointer to signals array.
         */
        SimulationSignals * simulationSignals() const;

    private:
        const gmx::Mdrunner* runner_ {nullptr};
};

}      // end namespace md
}      // end namespace gmx

#endif //GROMACS_CONTEXT_H

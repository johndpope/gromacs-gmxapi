//
// Created by Eric Irrgang on 10/25/17.
//

#ifndef GROMACS_RESTRAINT_MANAGER_H
#define GROMACS_RESTRAINT_MANAGER_H

/*! \libinternal \file
 * \brief Declare the Manager for restraint potentials.
 *
 * \ingroup module_restraint
 */

#include <memory>
#include <mutex>
#include <string>
#include "gromacs/utility/basedefinitions.h"
#include "restraintpotential.h"

struct t_commrec;
struct t_mdatoms;
struct pull_t;

namespace gmx
{
class LegacyPuller;

/*!
 * \brief Implementation details for MD restraints
 */
namespace restraint
{

class ManagerImpl;
class ICalculation;

//class ManagedForceProvider : public IForceProvider
//{
//        void calculateForces(const t_commrec *cr,
//                             const t_mdatoms *mdatoms,
//                             const real[3]
//
//        * box,
//        double t,
//        const rvec *x, gmx::ArrayRef<gmx::RVec>
//        force)
//        override;
//};

/*!
 * \brief Manage the Restraint potentials available for Molecular Dynamics.
 *
 * Until further factoring of the MD integrators and force calculations, we use a singleton
 * to reduce coupling between rapidly changing GROMACS components. Ultimately, this manager
 * should either not be necessary or can be used in more tightly scoped instances.
 *
 * The manager takes ownership of the "pull groups" (or atomic selections) and of
 * the various restraints and constraints applied for a given simulation.
 *
 * Calling code provides the manager with a means to access the various required input data
 * to be used when restraints are computed.
 *
 * \internal
 * When calculate(t) is called, the various input and output data sources are provided to
 * a CalculationBuilder to produce a Calculation object for the point in time, t.
 * Constructing the Calculation object triggers updates to the simulation state force array
 * and virial tensor. After construction, the Calculation object can be queried for calculated
 * data such as energy or pulling work.
 */
class Manager final
{
    public:
        ~Manager();

        /// Get a shared reference to the global manager.
        static std::shared_ptr<Manager> instance();

        Manager(const Manager&) = delete;
        Manager              &operator=(const Manager &) = delete;
        Manager(Manager &&) = delete;
        Manager              &operator=(Manager &&) = delete;

        /*!
         * \brief Clear registered restraints and reset the manager.
         */
        void clear() noexcept;

        /*!
         * \brief Get the number of currently managed restraints.
         *
         * \return number of restraints.
         *
         * \internal
         * Only considers the IRestraintPotential objects
         */
        unsigned long countRestraints() noexcept;

        /*! \brief Obtain the ability to create a restraint MDModule
         *
         * Though the name is reminiscent of the evolving idea of a work specification, the
         * Spec here is just a list of restraint modules.
         *
         * \param puller shared ownership of a restraint potential interface.
         * \param name key by which to reference the restraint.
         */
        void addToSpec(std::shared_ptr<gmx::IRestraintPotential> puller,
                       std::string                               name);

        /*!
         * \brief Get a copy of the current set of restraints to be applied.
         *
         * \return a copy of the list of restraint potentials.
         */
        std::vector < std::shared_ptr < IRestraintPotential>> getSpec() const;

    private:
        /// Private constructor enforces singleton pattern.
        Manager();

        /// Regulate initialization of the manager instance when the singleton is first accessed.
        static std::mutex initializationMutex_;

        /// Ownership of the shared reference to the global manager.
        static std::shared_ptr<Manager> instance_;

        /// Opaque implementation pointer.
        std::unique_ptr<ManagerImpl> impl_;
};


}      // end namespace restraint

}      // end namespace gmx

#endif //GROMACS_RESTRAINT_MANAGER_H

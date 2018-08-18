//
// Created by Eric Irrgang on 8/17/18.
//

#ifndef GROMACS_MD_IMPL_H
#define GROMACS_MD_IMPL_H

#include "md.h"

namespace gmx
{

class MDIntegrator::Impl
{
    public:
        /*!
         * \brief Extract non-owning fields from parameter structure.
         *
         * \param container previously initialized parameter structure.
         */
        explicit Impl(const IntegratorParamsContainer& container);

        ~Impl();

        //! Handles logging.
        FILE                            *fplog;
        //! Handles communication.
        t_commrec                       *cr;
        //! Coordinates multi-simulations.
        const gmx_multisim_t            *ms;
        //! Handles logging.
        const MDLogger                  &mdlog;
        //! Count of input file options.
        int                              nfile;
        //! Content of input file options.
        const t_filenm                  *fnm;
        //! Handles writing text output.
        const gmx_output_env_t          *oenv;
        //! Contains command-line options to mdrun.
        const MdrunOptions              &mdrunOptions;
        //! Handles virtual sites.
        gmx_vsite_t                     *vsite;
        //! Handles constraints.
        Constraints                     *constr;
        //! Handles enforced rotation.
        gmx_enfrot                      *enforcedRotation;
        //! Handles box deformation.
        BoxDeformation                  *deform;
        //! Handles writing output files.
        IMDOutputProvider               *outputProvider;
        //! Contains user input mdp options.
        t_inputrec                      *inputrec;
        //! Full system topology.
        gmx_mtop_t                      *top_global;
        //! Helper struct for force calculations.
        t_fcdata                        *fcd;
        //! Full simulation state (only non-nullptr on master rank).
        t_state                         *state_global;
        //! History of simulation observables.
        ObservablesHistory              *observablesHistory;
        //! Atom parameters for this domain.
        MDAtoms                         *mdAtoms;
        //! Manages flop accounting.
        t_nrnb                          *nrnb;
        //! Manages wall cycle accounting.
        gmx_wallcycle                   *wcycle;
        //! Parameters for force calculations.
        t_forcerec                      *fr;
        //! Parameters for replica exchange algorihtms.
        const ReplicaExchangeParameters &replExParams;
        //! Parameters for membrane embedding.
        gmx_membed_t                    *membed;
        //! Manages wall time accounting.
        gmx_walltime_accounting         *walltime_accounting;

        void run();
};

} // end namespace gmx

#endif //GROMACS_MD_IMPL_H

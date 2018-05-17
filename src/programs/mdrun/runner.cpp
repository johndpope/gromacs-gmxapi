/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 1991-2000, University of Groningen, The Netherlands.
 * Copyright (c) 2001-2004, The GROMACS development team.
 * Copyright (c) 2011,2012,2013,2014,2015,2016,2017, by the GROMACS development team, led by
 * Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
 * and including many others, as listed in the AUTHORS file in the
 * top-level source directory and at http://www.gromacs.org.
 *
 * GROMACS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * GROMACS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GROMACS; if not, see
 * http://www.gnu.org/licenses, or write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 *
 * If you want to redistribute modifications to GROMACS, please
 * consider that scientific software is very special. Version
 * control is crucial - bugs must be traceable. We will be happy to
 * consider code for inclusion in the official distribution, but
 * derived work must not be called official GROMACS. Details are found
 * in the README & COPYING files - if they are missing, get the
 * official version at http://www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the research papers on the package. Check out http://www.gromacs.org.
 */
/*! \internal \file
 *
 * \brief Implements the MD runner routine calling all integrators.
 *
 * \author David van der Spoel <david.vanderspoel@icm.uu.se>
 * \ingroup module_mdlib
 */
#include "gmxpre.h"

#include "runner.h"

#include "config.h"

#include <cassert>
#include <csignal>
#include <cstdlib>
#include <string>
#include <iostream>

#include <algorithm>
#include <gromacs/restraint/manager.h>
#include "gromacs/utility/arraysize.h"
#include "gromacs/mdrunutility/handlerestart.h"

#include "gromacs/commandline/filenm.h"
#include "gromacs/compat/make_unique.h"
#include "gromacs/domdec/domdec.h"
#include "gromacs/domdec/domdec_struct.h"
#include "gromacs/ewald/pme.h"
#include "gromacs/fileio/checkpoint.h"
#include "gromacs/fileio/oenv.h"
#include "gromacs/fileio/tpxio.h"
#include "gromacs/gmxlib/network.h"
#include "gromacs/gpu_utils/gpu_utils.h"
#include "gromacs/hardware/cpuinfo.h"
#include "gromacs/hardware/detecthardware.h"
#include "gromacs/hardware/hardwareassign.h"
#include "gromacs/hardware/printhardware.h"
#include "gromacs/listed-forces/disre.h"
#include "gromacs/listed-forces/orires.h"
#include "gromacs/math/calculate-ewald-splitting-coefficient.h"
#include "gromacs/math/functions.h"
#include "gromacs/math/utilities.h"
#include "gromacs/math/vec.h"
#include "gromacs/mdlib/calc_verletbuf.h"
#include "gromacs/mdlib/constr.h"
#include "gromacs/mdlib/force.h"
#include "gromacs/mdlib/forcerec.h"
#include "gromacs/mdlib/gmx_omp_nthreads.h"
#include "gromacs/mdlib/integrator.h"
#include "gromacs/mdlib/main.h"
#include "gromacs/mdlib/md_support.h"
#include "gromacs/mdlib/mdatoms.h"
#include "gromacs/mdlib/mdrun.h"
#include "gromacs/mdlib/minimize.h"
#include "gromacs/mdlib/nbnxn_search.h"
#include "gromacs/mdlib/nbnxn_tuning.h"
#include "gromacs/mdlib/qmmm.h"
#include "gromacs/mdlib/sighandler.h"
#include "gromacs/mdlib/sim_util.h"
#include "gromacs/mdlib/tpi.h"
#include "gromacs/mdrunutility/mdmodules.h"
#include "gromacs/mdrunutility/threadaffinity.h"
#include "gromacs/mdtypes/commrec.h"
#include "gromacs/mdtypes/edsamhistory.h"
#include "gromacs/mdtypes/energyhistory.h"
#include "gromacs/mdtypes/inputrec.h"
#include "gromacs/mdtypes/md_enums.h"
#include "gromacs/mdtypes/observableshistory.h"
#include "gromacs/mdtypes/state.h"
#include "gromacs/mdtypes/swaphistory.h"
#include "gromacs/mdtypes/tpxstate.h"
#include "gromacs/pbcutil/pbc.h"
#include "gromacs/pulling/pull.h"
#include "gromacs/restraint/restraintpotential.h"
#include "gromacs/restraint/restraintmdmodule.h"
#include "gromacs/pulling/pull_rotation.h"
#include "gromacs/timing/wallcycle.h"
#include "gromacs/topology/mtop_util.h"
#include "gromacs/trajectory/trajectoryframe.h"
#include "gromacs/utility/cstringutil.h"
#include "gromacs/utility/exceptions.h"
#include "gromacs/utility/fatalerror.h"
#include "gromacs/utility/filestream.h"
#include "gromacs/utility/gmxassert.h"
#include "gromacs/utility/gmxmpi.h"
#include "gromacs/utility/logger.h"
#include "gromacs/utility/loggerbuilder.h"
#include "gromacs/utility/pleasecite.h"
#include "gromacs/utility/programcontext.h"
#include "gromacs/utility/smalloc.h"

#include "deform.h"
#include "md.h"
#include "membed.h"
#include "repl_ex.h"
#include "resource-division.h"
#include "context.h"

#ifdef GMX_FAHCORE
#include "corewrap.h"
#endif

//! First step used in pressure scaling
gmx_int64_t         deform_init_init_step_tpx;
//! Initial box for pressure scaling
matrix              deform_init_box_tpx;
//! MPI variable for use in pressure scaling
tMPI_Thread_mutex_t deform_init_box_mutex = TMPI_THREAD_MUTEX_INITIALIZER;

template<typename T>
bool stringIsEmpty(T string)
{ return string == nullptr; };

template<>
bool stringIsEmpty<std::string>(std::string string)
{ return string.empty(); };

#if GMX_THREAD_MPI
/* The minimum number of atoms per tMPI thread. With fewer atoms than this,
 * the number of threads will get lowered.
 */
#define MIN_ATOMS_PER_MPI_THREAD    90
#define MIN_ATOMS_PER_GPU           900

namespace gmx
{

std::unique_ptr<Mdrunner> Mdrunner::cloneOnSpawnedThread() const
{
    auto newRunner = gmx::compat::make_unique<Mdrunner>();

    // Todo: how to handle the restraint manager or parameters not in inputrec?

    newRunner->hw_opt = hw_opt;
    // TODO This duplication is formally necessary if any thread might
    // modify any memory in fnm or the pointers it contains. If the
    // contents are ever provably const, then we can remove this
    // allocation (and memory leak).
    newRunner->fnm = dup_tfn(nfile, fnm);
    newRunner->oenv = oenv;
    newRunner->bVerbose = bVerbose;
    newRunner->nstglobalcomm = nstglobalcomm;
    newRunner->ddxyz[0] = ddxyz[0];
    newRunner->ddxyz[1] = ddxyz[1];
    newRunner->ddxyz[2] = ddxyz[2];
    newRunner->dd_rank_order = dd_rank_order;
    newRunner->npme = npme;
    newRunner->rdd = rdd;
    newRunner->rconstr = rconstr;
    newRunner->dddlb_opt = dddlb_opt;
    newRunner->dlb_scale = dlb_scale;
    newRunner->ddcsx = ddcsx;
    newRunner->ddcsy = ddcsy;
    newRunner->ddcsz = ddcsz;
    newRunner->nbpu_opt = nbpu_opt;
    newRunner->nstlist_cmdline = nstlist_cmdline;
    newRunner->nsteps_cmdline = nsteps_cmdline;
    newRunner->nstepout = nstepout;
    newRunner->resetstep = resetstep;
    newRunner->nmultisim = nmultisim;
    newRunner->replExParams = replExParams;
    newRunner->pforce = pforce;
    newRunner->cpt_period = cpt_period;
    newRunner->max_hours = max_hours;
    newRunner->imdport = imdport;
    newRunner->Flags = Flags;
    newRunner->cr  = reinitialize_commrec_for_this_thread(cr);
    // Don't copy fplog file pointer.

    return newRunner;
}

/*! \brief The callback used for running on spawned threads.
 *
 * Obtains the pointer to the master mdrunner object from the one
 * argument permitted to the thread-launch API call, copies it to make
 * a new runner for this thread, reinitializes necessary data, and
 * proceeds to the simulation. */
static void mdrunner_start_fn(void *arg)
{
    try
    {
        auto masterMdrunner = reinterpret_cast<const gmx::Mdrunner *>(arg);
        /* copy the arg list to make sure that it's thread-local. This
           doesn't copy pointed-to items, of course, but those are all
           const. */
        std::unique_ptr<gmx::Mdrunner> mdrunner = masterMdrunner->cloneOnSpawnedThread();
        mdrunner->mdrunner();
    }
    GMX_CATCH_ALL_AND_EXIT_WITH_FATAL_ERROR;
}


/*! \brief Start thread-MPI threads.
 *
 * Called by mdrunner() to start a specific number of threads
 * (including the main thread) for thread-parallel runs. This in turn
 * calls mdrunner() for each thread. All options are the same as for
 * mdrunner(). */
t_commrec *Mdrunner::spawnThreads(int numThreadsToLaunch)
{

    /* first check whether we even need to start tMPI */
    if (numThreadsToLaunch < 2)
    {
        return cr;
    }

    /* now spawn new threads that start mdrunner_start_fn(), while
       the main thread returns, we set thread affinity later */
    if (tMPI_Init_fn(TRUE, numThreadsToLaunch, TMPI_AFFINITY_NONE,
                     mdrunner_start_fn, static_cast<void*>(this)) != TMPI_SUCCESS)
    {
        GMX_THROW(gmx::InternalError("Failed to spawn thread-MPI threads"));
    }

    return reinitialize_commrec_for_this_thread(cr);
}

}      // namespace

#endif /* GMX_THREAD_MPI */

/*! \brief Initialize variables for Verlet scheme simulation */
static void prepare_verlet_scheme(FILE                           *fplog,
                                  t_commrec                      *cr,
                                  t_inputrec                     *ir,
                                  int                             nstlist_cmdline,
                                  const gmx_mtop_t               *mtop,
                                  const matrix                          box,
                                  bool                            makeGpuPairList,
                                  const gmx::CpuInfo             &cpuinfo)
{
    /* For NVE simulations, we will retain the initial list buffer */
    if (EI_DYNAMICS(ir->eI) &&
        ir->verletbuf_tol > 0 &&
        !(EI_MD(ir->eI) && ir->etc == etcNO))
    {
        /* Update the Verlet buffer size for the current run setup */
        verletbuf_list_setup_t ls;
        real                   rlist_new;

        /* Here we assume SIMD-enabled kernels are being used. But as currently
         * calc_verlet_buffer_size gives the same results for 4x8 and 4x4
         * and 4x2 gives a larger buffer than 4x4, this is ok.
         */
        verletbuf_get_list_setup(true, makeGpuPairList, &ls);

        calc_verlet_buffer_size(mtop, det(box), ir, ir->nstlist, ir->nstlist - 1, -1, &ls, nullptr, &rlist_new);

        if (rlist_new != ir->rlist)
        {
            if (fplog != nullptr)
            {
                fprintf(fplog, "\nChanging rlist from %g to %g for non-bonded %dx%d atom kernels\n\n",
                        ir->rlist, rlist_new,
                        ls.cluster_size_i, ls.cluster_size_j);
            }
            ir->rlist     = rlist_new;
        }
    }

    if (nstlist_cmdline > 0 && (!EI_DYNAMICS(ir->eI) || ir->verletbuf_tol <= 0))
    {
        gmx_fatal(FARGS, "Can not set nstlist without %s",
                  !EI_DYNAMICS(ir->eI) ? "dynamics" : "verlet-buffer-tolerance");
    }

    if (EI_DYNAMICS(ir->eI))
    {
        /* Set or try nstlist values */
        increase_nstlist(fplog, cr, ir, nstlist_cmdline, mtop, box, makeGpuPairList, cpuinfo);
    }
}

/*! \brief Override the nslist value in inputrec
 *
 * with value passed on the command line (if any)
 */
static void override_nsteps_cmdline(const gmx::MDLogger &mdlog,
                                    gmx_int64_t          nsteps_cmdline,
                                    t_inputrec          *ir)
{
    assert(ir);

    /* override with anything else than the default -2 */
    if (nsteps_cmdline > -2)
    {
        char sbuf_steps[STEPSTRSIZE];
        char sbuf_msg[STRLEN];

        ir->nsteps = nsteps_cmdline;
        if (EI_DYNAMICS(ir->eI) && nsteps_cmdline != -1)
        {
            sprintf(sbuf_msg, "Overriding nsteps with value passed on the command line: %s steps, %.3g ps",
                    gmx_step_str(nsteps_cmdline, sbuf_steps),
                    fabs(nsteps_cmdline*ir->delta_t));
        }
        else
        {
            sprintf(sbuf_msg, "Overriding nsteps with value passed on the command line: %s steps",
                    gmx_step_str(nsteps_cmdline, sbuf_steps));
        }

        GMX_LOG(mdlog.warning).asParagraph().appendText(sbuf_msg);
    }
    else if (nsteps_cmdline < -2)
    {
        gmx_fatal(FARGS, "Invalid nsteps value passed on the command line: %d",
                  nsteps_cmdline);
    }
    /* Do nothing if nsteps_cmdline == -2 */
}

void gmx::Mdrunner::initFromAPI(const std::vector<std::string>& args)
{
    if ((tpxState_ == nullptr) || (!tpxState_->isInitialized()))
    {
        gmx_fatal(FARGS, "Need initialized input record to initialize runner.");
    }
    // Until the options processing gets picked apart more (at least the fnm handling)
    // we're just spoofing argv and wrapping initFromCLI. Note that a non-const argv is deeply
    // embedded in GROMACS.
    constexpr int offset = 3; // need placeholders for argv[0] and tpr file
    int argc = offset + static_cast<int>(args.size());
    std::vector<char *> argv{static_cast<size_t>(argc)};

    argv[0] = new char[1]; // Start with an empty string (doesn't really matter)
    strcpy(argv[0], "");
    argv[1] = new char[3];
    strcpy(argv[1], "-s");
    argv[2] = new char[strlen(tpxState_->filename()) + 1];
    strcpy(argv[2], tpxState_->filename());
    argv[2][strlen(tpxState_->filename()) + 1] = 0;

    for(size_t idx_args = 0 ; idx_args < args.size(); ++idx_args)
    {
        auto idx_argv = idx_args + offset;
        argv[idx_argv] = new char[args[idx_args].length() + 1];
        strcpy(argv[idx_argv], args[idx_args].c_str());
    }

    initFromCLI(argc, argv.data());

    for(auto&& string : argv)
    {
        delete[] string;
        string = nullptr;
    }
}

void gmx::Mdrunner::initFromCLI(int argc, char *argv[])
{
    const char   *desc[] = {
        "[THISMODULE] is the main computational chemistry engine",
        "within GROMACS. Obviously, it performs Molecular Dynamics simulations,",
        "but it can also perform Stochastic Dynamics, Energy Minimization,",
        "test particle insertion or (re)calculation of energies.",
        "Normal mode analysis is another option. In this case [TT]mdrun[tt]",
        "builds a Hessian matrix from single conformation.",
        "For usual Normal Modes-like calculations, make sure that",
        "the structure provided is properly energy-minimized.",
        "The generated matrix can be diagonalized by [gmx-nmeig].[PAR]",
        "The [TT]mdrun[tt] program reads the run input file ([TT]-s[tt])",
        "and distributes the topology over ranks if needed.",
        "[TT]mdrun[tt] produces at least four output files.",
        "A single log file ([TT]-g[tt]) is written.",
        "The trajectory file ([TT]-o[tt]), contains coordinates, velocities and",
        "optionally forces.",
        "The structure file ([TT]-c[tt]) contains the coordinates and",
        "velocities of the last step.",
        "The energy file ([TT]-e[tt]) contains energies, the temperature,",
        "pressure, etc, a lot of these things are also printed in the log file.",
        "Optionally coordinates can be written to a compressed trajectory file",
        "([TT]-x[tt]).[PAR]",
        "The option [TT]-dhdl[tt] is only used when free energy calculation is",
        "turned on.[PAR]",
        "Running mdrun efficiently in parallel is a complex topic topic,",
        "many aspects of which are covered in the online User Guide. You",
        "should look there for practical advice on using many of the options",
        "available in mdrun.[PAR]",
        "ED (essential dynamics) sampling and/or additional flooding potentials",
        "are switched on by using the [TT]-ei[tt] flag followed by an [REF].edi[ref]",
        "file. The [REF].edi[ref] file can be produced with the [TT]make_edi[tt] tool",
        "or by using options in the essdyn menu of the WHAT IF program.",
        "[TT]mdrun[tt] produces a [REF].xvg[ref] output file that",
        "contains projections of positions, velocities and forces onto selected",
        "eigenvectors.[PAR]",
        "When user-defined potential functions have been selected in the",
        "[REF].mdp[ref] file the [TT]-table[tt] option is used to pass [TT]mdrun[tt]",
        "a formatted table with potential functions. The file is read from",
        "either the current directory or from the [TT]GMXLIB[tt] directory.",
        "A number of pre-formatted tables are presented in the [TT]GMXLIB[tt] dir,",
        "for 6-8, 6-9, 6-10, 6-11, 6-12 Lennard-Jones potentials with",
        "normal Coulomb.",
        "When pair interactions are present, a separate table for pair interaction",
        "functions is read using the [TT]-tablep[tt] option.[PAR]",
        "When tabulated bonded functions are present in the topology,",
        "interaction functions are read using the [TT]-tableb[tt] option.",
        "For each different tabulated interaction type used, a table file name must",
        "be given. For the topology to work, a file name given here must match a",
        "character sequence before the file extension. That sequence is: an underscore,",
        "then a 'b' for bonds, an 'a' for angles or a 'd' for dihedrals,",
        "and finally the matching table number index used in the topology.[PAR]",
        "The options [TT]-px[tt] and [TT]-pf[tt] are used for writing pull COM",
        "coordinates and forces when pulling is selected",
        "in the [REF].mdp[ref] file.[PAR]",
        "Finally some experimental algorithms can be tested when the",
        "appropriate options have been given. Currently under",
        "investigation are: polarizability.",
        "[PAR]",
        "The option [TT]-membed[tt] does what used to be g_membed, i.e. embed",
        "a protein into a membrane. This module requires a number of settings",
        "that are provided in a data file that is the argument of this option.",
        "For more details in membrane embedding, see the documentation in the",
        "user guide. The options [TT]-mn[tt] and [TT]-mp[tt] are used to provide",
        "the index and topology files used for the embedding.",
        "[PAR]",
        "The option [TT]-pforce[tt] is useful when you suspect a simulation",
        "crashes due to too large forces. With this option coordinates and",
        "forces of atoms with a force larger than a certain value will",
        "be printed to stderr. It will also terminate the run when non-finite",
        "forces are present.",
        "[PAR]",
        "Checkpoints containing the complete state of the system are written",
        "at regular intervals (option [TT]-cpt[tt]) to the file [TT]-cpo[tt],",
        "unless option [TT]-cpt[tt] is set to -1.",
        "The previous checkpoint is backed up to [TT]state_prev.cpt[tt] to",
        "make sure that a recent state of the system is always available,",
        "even when the simulation is terminated while writing a checkpoint.",
        "With [TT]-cpnum[tt] all checkpoint files are kept and appended",
        "with the step number.",
        "A simulation can be continued by reading the full state from file",
        "with option [TT]-cpi[tt]. This option is intelligent in the way that",
        "if no checkpoint file is found, GROMACS just assumes a normal run and",
        "starts from the first step of the [REF].tpr[ref] file. By default the output",
        "will be appending to the existing output files. The checkpoint file",
        "contains checksums of all output files, such that you will never",
        "loose data when some output files are modified, corrupt or removed.",
        "There are three scenarios with [TT]-cpi[tt]:[PAR]",
        "[TT]*[tt] no files with matching names are present: new output files are written[PAR]",
        "[TT]*[tt] all files are present with names and checksums matching those stored",
        "in the checkpoint file: files are appended[PAR]",
        "[TT]*[tt] otherwise no files are modified and a fatal error is generated[PAR]",
        "With [TT]-noappend[tt] new output files are opened and the simulation",
        "part number is added to all output file names.",
        "Note that in all cases the checkpoint file itself is not renamed",
        "and will be overwritten, unless its name does not match",
        "the [TT]-cpo[tt] option.",
        "[PAR]",
        "With checkpointing the output is appended to previously written",
        "output files, unless [TT]-noappend[tt] is used or none of the previous",
        "output files are present (except for the checkpoint file).",
        "The integrity of the files to be appended is verified using checksums",
        "which are stored in the checkpoint file. This ensures that output can",
        "not be mixed up or corrupted due to file appending. When only some",
        "of the previous output files are present, a fatal error is generated",
        "and no old output files are modified and no new output files are opened.",
        "The result with appending will be the same as from a single run.",
        "The contents will be binary identical, unless you use a different number",
        "of ranks or dynamic load balancing or the FFT library uses optimizations",
        "through timing.",
        "[PAR]",
        "With option [TT]-maxh[tt] a simulation is terminated and a checkpoint",
        "file is written at the first neighbor search step where the run time",
        "exceeds [TT]-maxh[tt]\\*0.99 hours. This option is particularly useful in",
        "combination with setting [TT]nsteps[tt] to -1 either in the mdp or using the",
        "similarly named command line option. This results in an infinite run,",
        "terminated only when the time limit set by [TT]-maxh[tt] is reached (if any)"
        "or upon receiving a signal."
        "[PAR]",
        "When [TT]mdrun[tt] receives a TERM signal, it will stop as soon as",
        "checkpoint file can be written, i.e. after the next global communication step.",
        "When [TT]mdrun[tt] receives an INT signal (e.g. when ctrl+C is",
        "pressed), it will stop at the next neighbor search step or at the",
        "second global communication step, whichever happens later.",
        "In both cases all the usual output will be written to file.",
        "When running with MPI, a signal to one of the [TT]mdrun[tt] ranks",
        "is sufficient, this signal should not be sent to mpirun or",
        "the [TT]mdrun[tt] process that is the parent of the others.",
        "[PAR]",
        "Interactive molecular dynamics (IMD) can be activated by using at least one",
        "of the three IMD switches: The [TT]-imdterm[tt] switch allows one to terminate",
        "the simulation from the molecular viewer (e.g. VMD). With [TT]-imdwait[tt],",
        "[TT]mdrun[tt] pauses whenever no IMD client is connected. Pulling from the",
        "IMD remote can be turned on by [TT]-imdpull[tt].",
        "The port [TT]mdrun[tt] listens to can be altered by [TT]-imdport[tt].The",
        "file pointed to by [TT]-if[tt] contains atom indices and forces if IMD",
        "pulling is used."
        "[PAR]",
        "When [TT]mdrun[tt] is started with MPI, it does not run niced by default."
    };

    /* Command line option parameters, with their default values */
    gmx_bool          bDoAppendFiles = Flags.test(appendFiles);
    gmx_bool          bDDBondCheck  = Flags.test(ddBondCheck);
    gmx_bool          bDDBondComm   = Flags.test(ddBondComm);
    gmx_bool          bTunePME      = Flags.test(tunePME);
    gmx_bool          bRerunVSite   = Flags.test(rerunVSite);
    gmx_bool          bConfout      = Flags.test(confOut);
    gmx_bool          bReproducible = Flags.test(reproducible);
    gmx_bool          bIMDwait      = Flags.test(imdWait);
    gmx_bool          bIMDterm      = Flags.test(imdTerm);
    gmx_bool          bIMDpull      = Flags.test(imdPull);

    /* Command line options */
    rvec              realddxyz                           = {0, 0, 0};
    const char       *ddrank_opt_choices[ddrankorderNR+1] =
    { nullptr, "interleave", "pp_pme", "cartesian", nullptr };
    const char       *dddlb_opt_choices[] =
    { nullptr, "auto", "no", "yes", nullptr };
    const char       *thread_aff_opt_choices[threadaffNR+1] =
    { nullptr, "auto", "on", "off", nullptr };
    const char       *nbpu_opt_choices[] =
    { nullptr, "auto", "cpu", "gpu", "gpu_cpu", nullptr };
    gmx_bool          bTryToAppendFiles     = TRUE;
    gmx_bool          bKeepAndNumCPT        = Flags.test(keepAndNumCpt);
    gmx_bool          bResetCountersHalfWay = Flags.test(resetCountersHalfWay);
    const char       *gpuIdTaskAssignment   = "";

    t_pargs           pa[] = {

        { "-dd",      FALSE, etRVEC, {&realddxyz},
          "Domain decomposition grid, 0 is optimize" },
        { "-ddorder", FALSE, etENUM, {ddrank_opt_choices},
          "DD rank order" },
        { "-npme",    FALSE, etINT, {&npme},
          "Number of separate ranks to be used for PME, -1 is guess" },
        { "-nt",      FALSE, etINT, {&hw_opt.nthreads_tot},
          "Total number of threads to start (0 is guess)" },
        { "-ntmpi",   FALSE, etINT, {&hw_opt.nthreads_tmpi},
          "Number of thread-MPI threads to start (0 is guess)" },
        { "-ntomp",   FALSE, etINT, {&hw_opt.nthreads_omp},
          "Number of OpenMP threads per MPI rank to start (0 is guess)" },
        { "-ntomp_pme", FALSE, etINT, {&hw_opt.nthreads_omp_pme},
          "Number of OpenMP threads per MPI rank to start (0 is -ntomp)" },
        { "-pin",     FALSE, etENUM, {thread_aff_opt_choices},
          "Whether mdrun should try to set thread affinities" },
        { "-pinoffset", FALSE, etINT, {&hw_opt.core_pinning_offset},
          "The lowest logical core number to which mdrun should pin the first thread" },
        { "-pinstride", FALSE, etINT, {&hw_opt.core_pinning_stride},
          "Pinning distance in logical cores for threads, use 0 to minimize the number of threads per physical core" },
        { "-gpu_id",  FALSE, etSTR, {&gpuIdTaskAssignment},
          "List of GPU device id-s to use, specifies the per-node PP rank to GPU mapping" },
        { "-ddcheck", FALSE, etBOOL, {&bDDBondCheck},
          "Check for all bonded interactions with DD" },
        { "-ddbondcomm", FALSE, etBOOL, {&bDDBondComm},
          "HIDDENUse special bonded atom communication when [TT]-rdd[tt] > cut-off" },
        { "-rdd",     FALSE, etREAL, {&rdd},
          "The maximum distance for bonded interactions with DD (nm), 0 is determine from initial coordinates" },
        { "-rcon",    FALSE, etREAL, {&rconstr},
          "Maximum distance for P-LINCS (nm), 0 is estimate" },
        { "-dlb",     FALSE, etENUM, {dddlb_opt_choices},
          "Dynamic load balancing (with DD)" },
        { "-dds",     FALSE, etREAL, {&dlb_scale},
          "Fraction in (0,1) by whose reciprocal the initial DD cell size will be increased in order to "
          "provide a margin in which dynamic load balancing can act while preserving the minimum cell size." },
        { "-ddcsx",   FALSE, etSTR, {&ddcsx},
          "HIDDENA string containing a vector of the relative sizes in the x "
          "direction of the corresponding DD cells. Only effective with static "
          "load balancing." },
        { "-ddcsy",   FALSE, etSTR, {&ddcsy},
          "HIDDENA string containing a vector of the relative sizes in the y "
          "direction of the corresponding DD cells. Only effective with static "
          "load balancing." },
        { "-ddcsz",   FALSE, etSTR, {&ddcsz},
          "HIDDENA string containing a vector of the relative sizes in the z "
          "direction of the corresponding DD cells. Only effective with static "
          "load balancing." },
        { "-gcom",    FALSE, etINT, {&nstglobalcomm},
          "Global communication frequency" },
        { "-nb",      FALSE, etENUM, {&nbpu_opt_choices},
          "Calculate non-bonded interactions on" },
        { "-nstlist", FALSE, etINT, {&nstlist_cmdline},
          "Set nstlist when using a Verlet buffer tolerance (0 is guess)" },
        { "-tunepme", FALSE, etBOOL, {&bTunePME},
          "Optimize PME load between PP/PME ranks or GPU/CPU" },
        { "-v",       FALSE, etBOOL, {&bVerbose},
          "Be loud and noisy" },
        { "-pforce",  FALSE, etREAL, {&pforce},
          "Print all forces larger than this (kJ/mol nm)" },
        { "-reprod",  FALSE, etBOOL, {&bReproducible},
          "Try to avoid optimizations that affect binary reproducibility" },
        { "-cpt",     FALSE, etREAL, {&cpt_period},
          "Checkpoint interval (minutes)" },
        { "-cpnum",   FALSE, etBOOL, {&bKeepAndNumCPT},
          "Keep and number checkpoint files" },
        { "-append",  FALSE, etBOOL, {&bTryToAppendFiles},
          "Append to previous output files when continuing from checkpoint instead of adding the simulation part number to all file names" },
        { "-nsteps",  FALSE, etINT64, {&nsteps_cmdline},
          "Run this number of steps, overrides .mdp file option (-1 means infinite, -2 means use mdp option, smaller is invalid)" },
        { "-maxh",   FALSE, etREAL, {&max_hours},
          "Terminate after 0.99 times this time (hours)" },
        { "-multi",   FALSE, etINT, {&nmultisim},
          "Do multiple simulations in parallel" },
        { "-replex",  FALSE, etINT, {&replExParams.exchangeInterval},
          "Attempt replica exchange periodically with this period (steps)" },
        { "-nex",  FALSE, etINT, {&replExParams.numExchanges},
          "Number of random exchanges to carry out each exchange interval (N^3 is one suggestion).  -nex zero or not specified gives neighbor replica exchange." },
        { "-reseed",  FALSE, etINT, {&replExParams.randomSeed},
          "Seed for replica exchange, -1 is generate a seed" },
        { "-imdport",    FALSE, etINT, {&imdport},
          "HIDDENIMD listening port" },
        { "-imdwait",  FALSE, etBOOL, {&bIMDwait},
          "HIDDENPause the simulation while no IMD client is connected" },
        { "-imdterm",  FALSE, etBOOL, {&bIMDterm},
          "HIDDENAllow termination of the simulation from IMD client" },
        { "-imdpull",  FALSE, etBOOL, {&bIMDpull},
          "HIDDENAllow pulling in the simulation from IMD client" },
        { "-rerunvsite", FALSE, etBOOL, {&bRerunVSite},
          "HIDDENRecalculate virtual site coordinates with [TT]-rerun[tt]" },
        { "-confout", FALSE, etBOOL, {&bConfout},
          "HIDDENWrite the last configuration with [TT]-c[tt] and force checkpointing at the last step" },
        { "-stepout", FALSE, etINT, {&nstepout},
          "HIDDENFrequency of writing the remaining wall clock time for the run" },
        { "-resetstep", FALSE, etINT, {&resetstep},
          "HIDDENReset cycle counters after these many time steps" },
        { "-resethway", FALSE, etBOOL, {&bResetCountersHalfWay},
          "HIDDENReset the cycle counters after half the number of steps or halfway [TT]-maxh[tt]" }
    };
    gmx_bool          bStartFromCpt = Flags.test(startFromCpt);
    char            **multidir = nullptr;

    unsigned long PCA_Flags = PCA_CAN_SET_DEFFNM;

    // With -multi or -multidir, the file names are going to get processed
    // further (or the working directory changed), so we can't check for their
    // existence during parsing.  It isn't useful to do any completion based on
    // file system contents, either.
    for (auto i = 0; i < argc; ++i)
    {
        // Check whether either of the command-line parameters that
        // will trigger a multi-simulation is set
        if (strcmp(argv[i], "-multi") == 0 || strcmp(argv[i], "-multidir") == 0)
        {
            PCA_Flags |= PCA_DISABLE_INPUT_FILE_CHECKING;;
        }
    }

    /* Comment this in to do fexist calls only on master
     * works not with rerun or tables at the moment
     * also comment out the version of init_forcerec in md.c
     * with NULL instead of opt2fn
     */
    /*
       if (!MASTER(cr))
       {
       PCA_Flags |= PCA_NOT_READ_NODE;
       }
     */

    // Initializes oenv; finishes filling in fnm
    if (!parse_common_args(&argc, argv, PCA_Flags, nfile, fnm, asize(pa), pa,
                           asize(desc), desc, 0, nullptr, &oenv))
    {
        sfree(cr);
        GMX_THROW(InvalidInputError("Could not parse command line arguments."));
    }

    // Handle the option that permits the user to select a GPU task
    // assignment, which could be in an environment variable (so that
    // there is a way to customize it, when using MPI in heterogeneous
    // contexts).
    {
        // TODO Argument parsing can't handle std::string. We should
        // fix that by changing the parsing, once more of the roles of
        // handling, validating and implementing defaults for user
        // command-line options have been seperated.
        hw_opt.gpuIdTaskAssignment = gpuIdTaskAssignment;
        const char *env = getenv("GMX_GPU_ID");
        if (env != nullptr)
        {
            if (!hw_opt.gpuIdTaskAssignment.empty())
            {
                gmx_fatal(FARGS, "GMX_GPU_ID and -gpu_id can not be used at the same time");
            }
            hw_opt.gpuIdTaskAssignment = env;
        }
    }

    dd_rank_order = nenum(ddrank_opt_choices);

    hw_opt.thread_affinity = nenum(thread_aff_opt_choices);

    /* now check the -multi and -multidir option */
    if (opt2bSet("-multidir", nfile, fnm))
    {
        if (nmultisim > 0)
        {
            gmx_fatal(FARGS, "mdrun -multi and -multidir options are mutually exclusive.");
        }
        nmultisim = opt2fns(&multidir, "-multidir", nfile, fnm);
    }


    if (replExParams.exchangeInterval != 0 && nmultisim < 2)
    {
        gmx_fatal(FARGS, "Need at least two replicas for replica exchange (option -multi)");
    }

    if (replExParams.numExchanges < 0)
    {
        gmx_fatal(FARGS, "Replica exchange number of exchanges needs to be positive");
    }

    if (nmultisim >= 1)
    {
#if !GMX_THREAD_MPI
        init_multisystem(cr, nmultisim, multidir, nfile, fnm);
#else
        gmx_fatal(FARGS, "mdrun -multi or -multidir are not supported with the thread-MPI library. "
                  "Please compile GROMACS with a proper external MPI library.");
#endif
    }

    if (!opt2bSet("-cpi", nfile, fnm))
    {
        // If we are not starting from a checkpoint we never allow files to be appended
        // to, since that has caused a ton of strange behaviour and bugs in the past.
        if (opt2parg_bSet("-append", asize(pa), pa))
        {
            // If the user explicitly used the -append option, explain that it is not possible.
            gmx_fatal(FARGS, "GROMACS can only append to files when restarting from a checkpoint.");
        }
        else
        {
            // If the user did not say anything explicit, just disable appending.
            bTryToAppendFiles = FALSE;
        }
    }

    handleRestart(cr, bTryToAppendFiles, nfile, fnm, &bDoAppendFiles, &bStartFromCpt);

    // Note: We cannot extract e.g. opt2parg_bSet("-append", asize(pa), pa) from this block.
    Flags.set(rerun, opt2bSet("-rerun", nfile, fnm));
    Flags.set(ddBondCheck, bDDBondCheck);
    Flags.set(ddBondComm, bDDBondComm);
    Flags.set(tunePME, bTunePME);
    Flags.set(confOut, bConfout);
    Flags.set(rerunVSite, bRerunVSite);
    Flags.set(reproducible, bReproducible);
    Flags.set(appendFiles, bDoAppendFiles);
    Flags.set(appendFilesSet, opt2parg_bSet("-append", asize(pa), pa));
    Flags.set(keepAndNumCpt, bKeepAndNumCPT);
    Flags.set(startFromCpt, bStartFromCpt);
    Flags.set(resetCountersHalfWay, bResetCountersHalfWay);
    Flags.set(ntompSet, opt2parg_bSet("-ntomp", asize(pa), pa));
    Flags.set(imdWait, bIMDwait);
    Flags.set(imdTerm, bIMDterm);
    Flags.set(imdPull, bIMDpull);

    ddxyz[XX] = (int)(realddxyz[XX] + 0.5);
    ddxyz[YY] = (int)(realddxyz[YY] + 0.5);
    ddxyz[ZZ] = (int)(realddxyz[ZZ] + 0.5);

    dddlb_opt = dddlb_opt_choices[0];
    nbpu_opt  = nbpu_opt_choices[0];

    /* We postpone opening the log file if we are appending, so we can
       first truncate the old log file and append to the correct position
       there instead.  */
    if (MASTER(cr) && !(Flags.test(appendFiles)))
    {
        gmx_log_open(ftp2fn(efLOG, nfile, fnm), cr,
                     Flags.test(appendFiles), &fplog);
    }
    else
    {
        fplog = nullptr;
    }
}

namespace gmx
{

//! Halt the run if there are inconsistences between user choices to run with GPUs and/or hardware detection.
static void exitIfCannotForceGpuRun(bool requirePhysicalGpu,
                                    bool emulateGpu,
                                    bool useVerletScheme,
                                    bool compatibleGpusFound)
{
    /* Was GPU acceleration either explicitly (-nb gpu) or implicitly
     * (gpu ID passed) requested? */
    if (!requirePhysicalGpu)
    {
        return;
    }

    if (GMX_GPU == GMX_GPU_NONE)
    {
        gmx_fatal(FARGS, "GPU acceleration requested, but %s was compiled without GPU support!",
                  gmx::getProgramContext().displayName());
    }

    if (emulateGpu)
    {
        gmx_fatal(FARGS, "GPU emulation cannot be requested together with GPU acceleration!");
    }

    if (!useVerletScheme)
    {
        gmx_fatal(FARGS, "GPU acceleration requested, but can't be used without cutoff-scheme=Verlet");
    }

    if (!compatibleGpusFound)
    {
        gmx_fatal(FARGS, "GPU acceleration requested, but no compatible GPUs were detected.");
    }
}

/*! \brief Return whether GPU acceleration is useful with the given settings.
 *
 * If not, logs a message about falling back to CPU code. */
static bool gpuAccelerationIsUseful(const MDLogger   &mdlog,
                                    const t_inputrec *ir,
                                    bool              doRerun)
{
    if (doRerun && ir->opts.ngener > 1)
    {
        /* Rerun execution time is dominated by I/O and pair search,
         * so GPUs are not very useful, plus they do not support more
         * than one energy group. If the user requested GPUs
         * explicitly, a fatal error is given later.  With non-reruns,
         * we fall back to a single whole-of system energy group
         * (which runs much faster than a multiple-energy-groups
         * implementation would), and issue a note in the .log
         * file. Users can re-run if they want the information. */
        GMX_LOG(mdlog.warning).asParagraph().appendText("Multiple energy groups is not implemented for GPUs, so is not useful for this rerun, so falling back to the CPU");
        return false;
    }

    return true;
}

//! \brief Return the correct integrator function.
static integrator_t *my_integrator(unsigned int ei)
{
    switch (ei)
    {
        case eiMD:
        case eiBD:
        case eiSD1:
        case eiVV:
        case eiVVAK:
            if (!EI_DYNAMICS(ei))
            {
                GMX_THROW(APIError("do_md integrator would be called for a non-dynamical integrator"));
            }
            return do_md;
        case eiSteep:
            return do_steep;
        case eiCG:
            return do_cg;
        case eiNM:
            return do_nm;
        case eiLBFGS:
            return do_lbfgs;
        case eiTPI:
        case eiTPIC:
            if (!EI_TPI(ei))
            {
                GMX_THROW(APIError("do_tpi integrator would be called for a non-TPI integrator"));
            }
            return do_tpi;
        case eiSD2_REMOVED:
            GMX_THROW(NotImplementedError("SD2 integrator has been removed"));
        default:
            GMX_THROW(APIError("Non existing integrator selected"));
    }
}

//! Initializes the logger for mdrun.
static gmx::LoggerOwner buildLogger(FILE *fplog, const t_commrec *cr)
{
    gmx::LoggerBuilder builder;
    if (fplog != nullptr)
    {
        builder.addTargetFile(gmx::MDLogger::LogLevel::Info, fplog);
    }
    if (cr == nullptr || SIMMASTER(cr))
    {
        builder.addTargetStream(gmx::MDLogger::LogLevel::Warning,
                                &gmx::TextOutputFile::standardError());
    }
    return builder.build();
}

int Mdrunner::mdrunner()
{
    matrix                    box;
    gmx_ddbox_t               ddbox = {0};
    int                       npme_major, npme_minor;
    t_nrnb                   *nrnb;
    t_mdatoms                *mdatoms       = nullptr;
    t_forcerec               *fr            = nullptr;
    t_fcdata                 *fcd           = nullptr;
    real                      ewaldcoeff_q  = 0;
    real                      ewaldcoeff_lj = 0;
    struct gmx_pme_t        **pmedata       = nullptr;
    gmx_vsite_t              *vsite         = nullptr;
    gmx_constr_t              constr;
    int                       nChargePerturbed = -1, nTypePerturbed = 0, status;
    gmx_wallcycle_t           wcycle;
    gmx_walltime_accounting_t walltime_accounting = nullptr;
    int                       rc;
    gmx_int64_t               reset_counters;
    int                       nthreads_pme = 1;
    gmx_membed_t *            membed       = nullptr;
    gmx_hw_info_t            *hwinfo       = nullptr;

    /* CAUTION: threads may be started later on in this function, so
       cr doesn't reflect the final parallel state right now */
    gmx::MDModules mdModules;

    bool doMembed = opt2bSet("-membed", nfile, fnm);
    bool doRerun  = Flags.test(rerun);

    /* Handle GPU-related user options. Later, we check consistency
     * with things like whether support is compiled, or tMPI thread
     * count. */
    bool emulateGpu            = getenv("GMX_EMULATE_GPU") != nullptr;
    bool forceUseCpu           = (strncmp(nbpu_opt, "cpu", 3) == 0);
    if (!hw_opt.gpuIdTaskAssignment.empty() && forceUseCpu)
    {
        gmx_fatal(FARGS, "GPU IDs were specified, and short-ranged interactions were assigned to the CPU. Make no more than one of these choices.");
    }
    bool forceUsePhysicalGpu = (strncmp(nbpu_opt, "gpu", 3) == 0) || !hw_opt.gpuIdTaskAssignment.empty();
    bool tryUsePhysicalGpu   = (strncmp(nbpu_opt, "auto", 4) == 0) && !emulateGpu && (GMX_GPU != GMX_GPU_NONE);

    if (Flags.test(appendFiles))
    {
        // If we are appending, we will get the filehandle another way
        fplog = nullptr;
    }
    // Here we assume that SIMMASTER(cr) does not change even after the
    // threads are started.
    gmx::LoggerOwner logOwner(buildLogger(fplog, cr));
    gmx::MDLogger    mdlog(logOwner.logger());

    /* Detect hardware, gather information. This is an operation that is
     * global for this process (MPI rank). */
    bool detectGpus = forceUsePhysicalGpu || tryUsePhysicalGpu;
    hwinfo = gmx_detect_hardware(mdlog, cr, detectGpus);

    gmx_print_detected_hardware(fplog, cr, mdlog, hwinfo);

    if (fplog != nullptr)
    {
        /* Print references after all software/hardware printing */
        please_cite(fplog, "Abraham2015");
        please_cite(fplog, "Pall2015");
        please_cite(fplog, "Pronk2013");
        please_cite(fplog, "Hess2008b");
        please_cite(fplog, "Spoel2005a");
        please_cite(fplog, "Lindahl2001a");
        please_cite(fplog, "Berendsen95a");
    }

    if (tpxState_ == nullptr)
    {
        // Todo: move to Mdrunner constructor
        tpxState_ = std::make_shared<TpxState>();
    }
    if (SIMMASTER(cr))
    {
        /* Read (nearly) all data required for the simulation */
        const char* filename = ftp2fn(efTPR, nfile, fnm);
        if (!stringIsEmpty(filename) && !tpxState_->isInitialized())
        {
            // Todo: move out of mdrunner() to a setup routine.
            // e.g.
            //    tpxState_ = TpxState::fromFile(filename); // in Mdrunner::mainFunction()
            //    ...
            //    tpxState_.extractDataLocally(); // here
            //    // or just rely on communicator to let TpxState decide what to do when getters are called.
            tpxState_ = TpxState::initializeFromFile(filename);
        }
    }

    t_inputrec    *inputrec = tpxState_->getRawInputrec();
    gmx_mtop_t* mtop = tpxState_->getRawMtop();
    t_state *                state         = tpxState_->getRawState();

    if (SIMMASTER(cr))
    {
        exitIfCannotForceGpuRun(forceUsePhysicalGpu,
                                emulateGpu,
                                inputrec->cutoff_scheme == ecutsVERLET,
                                compatibleGpusFound(hwinfo->gpu_info));

        if (inputrec->cutoff_scheme == ecutsVERLET)
        {
            /* TODO This logic could run later, e.g. before -npme -1
               is handled. If inputrec has already been communicated,
               then the resulting tryUsePhysicalGpu does not need to
               be communicated. */
            if ((tryUsePhysicalGpu || forceUsePhysicalGpu) &&
                !gpuAccelerationIsUseful(mdlog, inputrec, doRerun))
            {
                /* Fallback message printed by nbnxn_acceleration_supported */
                if (forceUsePhysicalGpu)
                {
                    gmx_fatal(FARGS, "GPU acceleration requested, but not supported with the given input settings");
                }
                tryUsePhysicalGpu = false;
            }
            /* TODO This logic could run later, e.g. after inputrec,
               mtop, and state have been communicated, but before DD
               is initialized. In particular, -ntmpi 0 only needs to
               know whether the Verlet scheme is active in order to
               choose the number of ranks (because GPUs might be
               usable).*/
            bool makeGpuPairList = (forceUsePhysicalGpu ||
                                    tryUsePhysicalGpu ||
                                    emulateGpu);
            prepare_verlet_scheme(fplog, cr,
                                  inputrec, nstlist_cmdline, mtop, state->box,
                                  makeGpuPairList, *hwinfo->cpuInfo);
        }
        else
        {
            if (nstlist_cmdline > 0)
            {
                gmx_fatal(FARGS, "Can not set nstlist with the group cut-off scheme");
            }

            if (compatibleGpusFound(hwinfo->gpu_info))
            {
                GMX_LOG(mdlog.warning).asParagraph().appendText(
                        "NOTE: GPU(s) found, but the current simulation can not use GPUs\n"
                        "      To use a GPU, set the mdp option: cutoff-scheme = Verlet");
                tryUsePhysicalGpu = false;
            }

#if GMX_TARGET_BGQ
            md_print_warn(cr, fplog,
                          "NOTE: There is no SIMD implementation of the group scheme kernels on\n"
                          "      BlueGene/Q. You will observe better performance from using the\n"
                          "      Verlet cut-off scheme.\n");
#endif
        }
    }

    /* Check and update the hardware options for internal consistency */
    check_and_update_hw_opt_1(&hw_opt, cr, npme);

    /* Early check for externally set process affinity. */
    gmx_check_thread_affinity_set(mdlog, cr,
                                  &hw_opt, hwinfo->nthreads_hw_avail, FALSE);

#if GMX_THREAD_MPI
    if (SIMMASTER(cr))
    {
        if (npme > 0 && hw_opt.nthreads_tmpi <= 0)
        {
            gmx_fatal(FARGS, "You need to explicitly specify the number of MPI threads (-ntmpi) when using separate PME ranks");
        }

        /* Since the master knows the cut-off scheme, update hw_opt for this.
         * This is done later for normal MPI and also once more with tMPI
         * for all tMPI ranks.
         */
        check_and_update_hw_opt_2(&hw_opt, inputrec->cutoff_scheme);

        /* Determine how many thread-MPI ranks to start.
         *
         * TODO Over-writing the user-supplied value here does
         * prevent any possible subsequent checks from working
         * correctly. */
        hw_opt.nthreads_tmpi = get_nthreads_mpi(hwinfo,
                                                &hw_opt,
                                                inputrec, mtop,
                                                mdlog,
                                                doMembed);

        // Now start the threads for thread MPI.
        cr = spawnThreads(hw_opt.nthreads_tmpi);
        /* The main thread continues here with a new cr. We don't deallocate
           the old cr because other threads may still be reading it. */
        // TODO Both master and spawned threads call dup_tfn and
        // reinitialize_commrec_for_this_thread. Find a way to express
        // this better.
    }
#endif
    /* END OF CAUTION: cr is now reliable */

    if (PAR(cr))
    {
        /* now broadcast everything to the non-master nodes/threads: */
        init_parallel(cr, inputrec, mtop);

        gmx_bcast_sim(sizeof(tryUsePhysicalGpu), &tryUsePhysicalGpu, cr);
    }

    // Build modules on all threads.
    {
        // Build restraints.
        // Currently there is at most one restraint modules.
        auto pullers = restraintManager_->getSpec();
        if (!pullers.empty())
        {
            for (auto&& puller : pullers)
            {
                auto module = ::gmx::RestraintMDModule::create(puller,
                                                               puller->sites());
                mdModules.add(std::move(module));
            }

            // Temporarily abuse the intention of the restraintManager and let
            // the restraints register more than just mdModules.

            // collect ControlModules for modules that want to provide a stop condition.
            for (auto&& puller : pullers)
            {
                puller->bindRunner(this);
            }
        }
    }
    // TODO: Error handling
    mdModules.assignOptionsToModules(*inputrec->params, nullptr);

    if (fplog != nullptr)
    {
        pr_inputrec(fplog, 0, "Input Parameters", inputrec, FALSE);
        fprintf(fplog, "\n");
    }

    /* now make sure the state is initialized and propagated */
    set_state_entries(state, inputrec);

    /* A parallel command line option consistency check that we can
       only do after any threads have started. */
    if (!PAR(cr) &&
        (ddxyz[XX] > 1 || ddxyz[YY] > 1 || ddxyz[ZZ] > 1 || npme > 0))
    {
        gmx_fatal(FARGS,
                  "The -dd or -npme option request a parallel simulation, "
#if !GMX_MPI
                  "but %s was compiled without threads or MPI enabled"
#else
#if GMX_THREAD_MPI
                  "but the number of MPI-threads (option -ntmpi) is not set or is 1"
#else
                  "but %s was not started through mpirun/mpiexec or only one rank was requested through mpirun/mpiexec"
#endif
#endif
                  , output_env_get_program_display_name(oenv)
                  );
    }

    if (doRerun &&
        (EI_ENERGY_MINIMIZATION(inputrec->eI) || eiNM == inputrec->eI))
    {
        gmx_fatal(FARGS, "The .mdp file specified an energy mininization or normal mode algorithm, and these are not compatible with mdrun -rerun");
    }

    if (can_use_allvsall(inputrec, TRUE, cr, fplog) && DOMAINDECOMP(cr))
    {
        gmx_fatal(FARGS, "All-vs-all loops do not work with domain decomposition, use a single MPI rank");
    }

    if (!(EEL_PME(inputrec->coulombtype) || EVDW_PME(inputrec->vdwtype)))
    {
        if (npme > 0)
        {
            gmx_fatal_collective(FARGS, cr->mpi_comm_mysim, MASTER(cr),
                                 "PME-only ranks are requested, but the system does not use PME for electrostatics or LJ");
        }

        npme = 0;
    }

    if ((tryUsePhysicalGpu || forceUsePhysicalGpu) && npme < 0)
    {
        /* With GPUs we don't automatically use PME-only ranks. PME ranks can
         * improve performance with many threads per GPU, since our OpenMP
         * scaling is bad, but it's difficult to automate the setup.
         */
        npme = 0;
    }

#ifdef GMX_FAHCORE
    if (MASTER(cr))
    {
        fcRegisterSteps(inputrec->nsteps, inputrec->init_step);
    }
#endif

    /* NMR restraints must be initialized before load_checkpoint,
     * since with time averaging the history is added to t_state.
     * For proper consistency check we therefore need to extend
     * t_state here.
     * So the PME-only nodes (if present) will also initialize
     * the distance restraints.
     */
    snew(fcd, 1);

    /* This needs to be called before read_checkpoint to extend the state */
    init_disres(fplog, mtop, inputrec, cr, fcd, state, replExParams.exchangeInterval > 0);

    init_orires(fplog, mtop, as_rvec_array(state->x.data()), inputrec, cr, &(fcd->orires),
                state);

    if (inputrecDeform(inputrec))
    {
        /* Store the deform reference box before reading the checkpoint */
        if (SIMMASTER(cr))
        {
            copy_mat(state->box, box);
        }
        if (PAR(cr))
        {
            gmx_bcast(sizeof(box), box, cr);
        }
        /* Because we do not have the update struct available yet
         * in which the reference values should be stored,
         * we store them temporarily in static variables.
         * This should be thread safe, since they are only written once
         * and with identical values.
         */
        tMPI_Thread_mutex_lock(&deform_init_box_mutex);
        deform_init_init_step_tpx = inputrec->init_step;
        copy_mat(box, deform_init_box_tpx);
        tMPI_Thread_mutex_unlock(&deform_init_box_mutex);
    }

    ObservablesHistory observablesHistory = {};

    if (Flags.test(startFromCpt))
    {
        /* Check if checkpoint file exists before doing continuation.
         * This way we can use identical input options for the first and subsequent runs...
         */
        gmx_bool bReadEkin;

        load_checkpoint(opt2fn_master("-cpi", nfile, fnm, cr), &fplog,
                        cr, ddxyz, &npme,
                        inputrec, state, &bReadEkin, &observablesHistory,
                        Flags.test(appendFiles),
                        Flags.test(appendFilesSet),
                        Flags.test(reproducible));

        if (bReadEkin)
        {
            Flags |= MD_READ_EKIN;
        }
    }

    if (SIMMASTER(cr) && Flags.test(appendFiles))
    {
        gmx_log_open(ftp2fn(efLOG, nfile, fnm), cr,
                     Flags.to_ulong(), &fplog);
        logOwner = buildLogger(fplog, nullptr);
        mdlog    = logOwner.logger();
    }

    /* override nsteps with value from cmdline */
    override_nsteps_cmdline(mdlog, nsteps_cmdline, inputrec);

    if (SIMMASTER(cr))
    {
        copy_mat(state->box, box);
    }

    if (PAR(cr))
    {
        gmx_bcast(sizeof(box), box, cr);
    }

    if (PAR(cr) && !(EI_TPI(inputrec->eI) ||
                     inputrec->eI == eiNM))
    {
        cr->dd = init_domain_decomposition(fplog, cr,
                                           Flags.to_ulong(),
                                           ddxyz, npme,
                                           dd_rank_order,
                                           rdd, rconstr,
                                           dddlb_opt, dlb_scale,
                                           ddcsx, ddcsy, ddcsz,
                                           mtop, inputrec,
                                           box, as_rvec_array(state->x.data()),
                                           &ddbox, &npme_major, &npme_minor);
    }
    else
    {
        /* PME, if used, is done on all nodes with 1D decomposition */
        cr->npmenodes = 0;
        cr->duty      = (DUTY_PP | DUTY_PME);
        npme_major    = 1;
        npme_minor    = 1;

        if (inputrec->ePBC == epbcSCREW)
        {
            gmx_fatal(FARGS,
                      "pbc=%s is only implemented with domain decomposition",
                      epbc_names[inputrec->ePBC]);
        }
    }

    if (PAR(cr))
    {
        /* After possible communicator splitting in make_dd_communicators.
         * we can set up the intra/inter node communication.
         */
        gmx_setup_nodecomm(fplog, cr);
    }

    /* Initialize per-physical-node MPI process/thread ID and counters. */
    gmx_init_intranode_counters(cr);
#if GMX_MPI
    if (MULTISIM(cr))
    {
        GMX_LOG(mdlog.warning).asParagraph().appendTextFormatted(
                "This is simulation %d out of %d running as a composite GROMACS\n"
                "multi-simulation job. Setup for this simulation:\n",
                cr->ms->sim, cr->ms->nsim);
    }
    GMX_LOG(mdlog.warning).appendTextFormatted(
            "Using %d MPI %s\n",
            cr->nnodes,
#if GMX_THREAD_MPI
            cr->nnodes == 1 ? "thread" : "threads"
#else
            cr->nnodes == 1 ? "process" : "processes"
#endif
            );
    fflush(stderr);
#endif

    /* Check and update hw_opt for the cut-off scheme */
    check_and_update_hw_opt_2(&hw_opt, inputrec->cutoff_scheme);

    /* Check and update hw_opt for the number of MPI ranks */
    check_and_update_hw_opt_3(&hw_opt);

    gmx_omp_nthreads_init(mdlog, cr,
                          hwinfo->nthreads_hw_avail,
                          hw_opt.nthreads_omp,
                          hw_opt.nthreads_omp_pme,
                          (cr->duty & DUTY_PP) == 0,
                          inputrec->cutoff_scheme == ecutsVERLET);

#ifndef NDEBUG
    if (EI_TPI(inputrec->eI) &&
        inputrec->cutoff_scheme == ecutsVERLET)
    {
        gmx_feenableexcept();
    }
#endif

    // Contains the ID of the GPU used by each PP rank on this node,
    // indexed by that rank. Empty if no GPUs are selected for use on
    // this node.
    std::vector<int> gpuTaskAssignment;
    if (tryUsePhysicalGpu || forceUsePhysicalGpu)
    {
        /* Currently the DD code assigns duty to ranks that can
         * include PP work that currently can be executed on a single
         * GPU, if present and compatible.  This has to be coordinated
         * across PP ranks on a node, with possible multiple devices
         * or sharing devices on a node, either from the user
         * selection, or automatically. */
        bool rankCanUseGpu = cr->duty & DUTY_PP;
        gpuTaskAssignment = mapPpRanksToGpus(rankCanUseGpu, cr, hwinfo->gpu_info, hw_opt);
    }

    reportGpuUsage(mdlog, hwinfo->gpu_info, !hw_opt.gpuIdTaskAssignment.empty(),
                   gpuTaskAssignment, cr->nrank_pp_intranode, cr->nnodes > 1);

    if (!gpuTaskAssignment.empty())
    {
        GMX_RELEASE_ASSERT(cr->nrank_pp_intranode == static_cast<int>(gpuTaskAssignment.size()),
                           "The number of PP ranks on each node must equal the number of GPU tasks used on each node");
    }

    /* Prevent other ranks from continuing after an issue was found
     * and reported as a fatal error.
     *
     * TODO This function implements a barrier so that MPI runtimes
     * can organize an orderly shutdown if one of the ranks has had to
     * issue a fatal error in various code already run. When we have
     * MPI-aware error handling and reporting, this should be
     * improved. */
#if GMX_MPI
    if (PAR(cr))
    {
        MPI_Barrier(cr->mpi_comm_mysim);
    }
#endif

    /* Now that we know the setup is consistent, check for efficiency */
    check_resource_division_efficiency(hwinfo, hw_opt.nthreads_tot, !gpuTaskAssignment.empty(), Flags.test(ntompSet),
                                       cr, mdlog);

    gmx_device_info_t *shortRangedDeviceInfo = nullptr;
    int                shortRangedDeviceId   = -1;
    if (cr->duty & DUTY_PP)
    {
        if (!gpuTaskAssignment.empty())
        {
            shortRangedDeviceId   = gpuTaskAssignment[cr->rank_pp_intranode];
            shortRangedDeviceInfo = getDeviceInfo(hwinfo->gpu_info, shortRangedDeviceId);
        }
    }

    if (DOMAINDECOMP(cr))
    {
        /* When we share GPUs over ranks, we need to know this for the DLB */
        dd_setup_dlb_resource_sharing(cr, shortRangedDeviceId);
    }

    /* getting number of PP/PME threads
       PME: env variable should be read only on one node to make sure it is
       identical everywhere;
     */
    nthreads_pme = gmx_omp_nthreads_get(emntPME);

    wcycle = wallcycle_init(fplog, resetstep, cr);

    if (PAR(cr))
    {
        /* Master synchronizes its value of reset_counters with all nodes
         * including PME only nodes */
        reset_counters = wcycle_get_reset_counters(wcycle);
        gmx_bcast_sim(sizeof(reset_counters), &reset_counters, cr);
        wcycle_set_reset_counters(wcycle, reset_counters);
    }

    // Membrane embedding must be initialized before we call init_forcerec()
    if (doMembed)
    {
        if (MASTER(cr))
        {
            fprintf(stderr, "Initializing membed");
        }
        /* Note that membed cannot work in parallel because mtop is
         * changed here. Fix this if we ever want to make it run with
         * multiple ranks. */
        membed = init_membed(fplog, nfile, fnm, mtop, inputrec, state, cr, &cpt_period);
    }

    snew(nrnb, 1);
    if (cr->duty & DUTY_PP)
    {
        bcast_state(cr, state);

        /* Initiate forcerecord */
        fr                 = mk_forcerec();
        fr->forceProviders = mdModules.initForceProviders();
        // Threads have been launched and DD initialized
        // Todo: restraintManager can provide a proper IMDModule interface later.
//        fr->forceProviders->addForceProvider(restraintManager_);
        init_forcerec(fplog, mdlog, fr, fcd,
                      inputrec, mtop, cr, box,
                      opt2fn("-table", nfile, fnm),
                      opt2fn("-tablep", nfile, fnm),
                      getFilenm("-tableb", nfile, fnm),
                      nbpu_opt,
                      shortRangedDeviceInfo,
                      FALSE,
                      pforce);

        /* Initialize QM-MM */
        if (fr->bQMMM)
        {
            init_QMMMrec(cr, mtop, inputrec, fr);
        }

        /* Initialize the mdatoms structure.
         * mdatoms is not filled with atom data,
         * as this can not be done now with domain decomposition.
         */
        mdatoms = init_mdatoms(fplog, mtop, inputrec->efep != efepNO);

        /* Initialize the virtual site communication */
        vsite = init_vsite(mtop, cr, FALSE);

        calc_shifts(box, fr->shift_vec);

        /* With periodic molecules the charge groups should be whole at start up
         * and the virtual sites should not be far from their proper positions.
         */
        if (!inputrec->bContinuation && MASTER(cr) &&
            !(inputrec->ePBC != epbcNONE && inputrec->bPeriodicMols))
        {
            /* Make molecules whole at start of run */
            if (fr->ePBC != epbcNONE)
            {
                do_pbc_first_mtop(fplog, inputrec->ePBC, box, mtop, as_rvec_array(state->x.data()));
            }
            if (vsite)
            {
                /* Correct initial vsite positions are required
                 * for the initial distribution in the domain decomposition
                 * and for the initial shell prediction.
                 */
                construct_vsites_mtop(vsite, mtop, as_rvec_array(state->x.data()));
            }
        }

        if (EEL_PME(fr->eeltype) || EVDW_PME(fr->vdwtype))
        {
            ewaldcoeff_q  = fr->ewaldcoeff_q;
            ewaldcoeff_lj = fr->ewaldcoeff_lj;
            pmedata       = &fr->pmedata;
        }
        else
        {
            pmedata = nullptr;
        }
    }
    else
    {
        /* This is a PME only node */

        /* We don't need the state */
        state         = nullptr;

        ewaldcoeff_q  = calc_ewaldcoeff_q(inputrec->rcoulomb, inputrec->ewald_rtol);
        ewaldcoeff_lj = calc_ewaldcoeff_lj(inputrec->rvdw, inputrec->ewald_rtol_lj);
        snew(pmedata, 1);
    }

    if (hw_opt.thread_affinity != threadaffOFF)
    {
        /* Before setting affinity, check whether the affinity has changed
         * - which indicates that probably the OpenMP library has changed it
         * since we first checked).
         */
        gmx_check_thread_affinity_set(mdlog, cr,
                                      &hw_opt, hwinfo->nthreads_hw_avail, TRUE);

        int nthread_local;
        /* threads on this MPI process or TMPI thread */
        if (cr->duty & DUTY_PP)
        {
            nthread_local = gmx_omp_nthreads_get(emntNonbonded);
        }
        else
        {
            nthread_local = gmx_omp_nthreads_get(emntPME);
        }

        /* Set the CPU affinity */
        gmx_set_thread_affinity(mdlog, cr, &hw_opt, *hwinfo->hardwareTopology,
                                nthread_local, nullptr);
    }

    /* Initiate PME if necessary,
     * either on all nodes or on dedicated PME nodes only. */
    if (EEL_PME(inputrec->coulombtype) || EVDW_PME(inputrec->vdwtype))
    {
        if (mdatoms)
        {
            nChargePerturbed = mdatoms->nChargePerturbed;
            if (EVDW_PME(inputrec->vdwtype))
            {
                nTypePerturbed   = mdatoms->nTypePerturbed;
            }
        }
        if (cr->npmenodes > 0)
        {
            /* The PME only nodes need to know nChargePerturbed(FEP on Q) and nTypePerturbed(FEP on LJ)*/
            gmx_bcast_sim(sizeof(nChargePerturbed), &nChargePerturbed, cr);
            gmx_bcast_sim(sizeof(nTypePerturbed), &nTypePerturbed, cr);
        }

        if (cr->duty & DUTY_PME)
        {
            try
            {
                status = gmx_pme_init(pmedata, cr, npme_major, npme_minor, inputrec,
                                      mtop ? mtop->natoms : 0, nChargePerturbed, nTypePerturbed,
                                      Flags.test(reproducible),
                                      ewaldcoeff_q, ewaldcoeff_lj,
                                      nthreads_pme);
            }
            GMX_CATCH_ALL_AND_EXIT_WITH_FATAL_ERROR;
            if (status != 0)
            {
                gmx_fatal(FARGS, "Error %d initializing PME", status);
            }
        }
    }


    if (EI_DYNAMICS(inputrec->eI))
    {
        /* Turn on signal handling on all nodes */
        /*
         * (A user signal from the PME nodes (if any)
         * is communicated to the PP nodes.
         */
        signal_handler_install();
    }

    if (cr->duty & DUTY_PP)
    {
        /* Assumes uniform use of the number of OpenMP threads */
        walltime_accounting = walltime_accounting_init(gmx_omp_nthreads_get(emntDefault));

        // If old MDP traditional MDP pulling options were used, the pull code
        // wrapped up in gmx::LegacyPullPack can be used.
        if (inputrec->bPull && inputrec->pull != nullptr)
        {
            // TODO: move to constructor when initializing runner is decoupled from reading TPR.
            /* Initialize pull code structures */
            auto pull_work =
                init_pull(fplog, inputrec->pull, inputrec, nfile, fnm,
                          mtop, cr, oenv, real(inputrec->fepvals->init_lambda),
                          EI_DYNAMICS(inputrec->eI) && MASTER(cr), Flags.to_ulong());
            auto legacyPullers = gmx::compat::make_unique<gmx::LegacyPuller>(pull_work);
            auto restraints = gmx::restraint::Manager::instance();
            // Maybe the error is here. If the results of init_pull are different on each thread,
            // then they probably get merged accidentally here.
            restraints->add(std::move(legacyPullers), std::string("old"));
        }
        // If we need an initialization hook, we can put it here.
        //pullers_->startRun();

        if (inputrec->bRot)
        {
            /* Initialize enforced rotation code */
            init_rot(fplog, inputrec, nfile, fnm, cr, as_rvec_array(state->x.data()), state->box, mtop, oenv,
                     bVerbose, Flags.to_ulong());
        }

        /* Let init_constraints know whether we have essential dynamics constraints.
         * TODO: inputrec should tell us whether we use an algorithm, not a file option or the checkpoint
         */
        bool doEdsam = (opt2fn_null("-ei", nfile, fnm) != nullptr || observablesHistory.edsamHistory);

        constr = init_constraints(fplog, mtop, inputrec, doEdsam, cr);

        if (DOMAINDECOMP(cr))
        {
            GMX_RELEASE_ASSERT(fr, "fr was NULL while cr->duty was DUTY_PP");
            /* This call is not included in init_domain_decomposition mainly
             * because fr->cginfo_mb is set later.
             */
            dd_init_bondeds(fplog, cr->dd, mtop, vsite, inputrec,
                            Flags.test(ddBondCheck), fr->cginfo_mb);
        }

        auto context = gmx::md::Context(*this);
        /* Now do whatever the user wants us to do (how flexible...) */
        auto integrator = my_integrator(inputrec->eI);
        integrator(fplog,
                   cr,
                   mdlog,
                   nfile,
                   fnm,
                   oenv,
                   bVerbose,
                   nstglobalcomm,
                   vsite,
                   constr,
                   nstepout,
                   mdModules.outputProvider(),
                   inputrec,
                   mtop,
                   fcd,
                   state,
                   &observablesHistory,
                   mdatoms,
                   nrnb,
                   wcycle,
                   fr,
                   replExParams,
                   membed,
                   cpt_period,
                   max_hours,
                   imdport,
                   Flags.to_ulong(),
                   walltime_accounting,
                   context);

        if (inputrec->bRot)
        {
            finish_rot(inputrec->rot);
        }

        if (inputrec->bPull)
        {
            auto puller = gmx::restraint::Manager::instance();
            puller->finish();
        }

    }
    else
    {
        GMX_RELEASE_ASSERT(pmedata, "pmedata was NULL while cr->duty was not DUTY_PP");
        /* do PME only */
        walltime_accounting = walltime_accounting_init(gmx_omp_nthreads_get(emntPME));
        gmx_pmeonly(*pmedata, cr, nrnb, wcycle, walltime_accounting, ewaldcoeff_q, ewaldcoeff_lj, inputrec);
    }

    wallcycle_stop(wcycle, ewcRUN);

    /* Finish up, write some stuff
     * if rerunMD, don't write last frame again
     */
    finish_run(fplog, mdlog, cr,
               inputrec, nrnb, wcycle, walltime_accounting,
               fr ? fr->nbv : nullptr,
               EI_DYNAMICS(inputrec->eI) && !MULTISIM(cr));

    // Free PME data
    if (pmedata)
    {
        gmx_pme_destroy(*pmedata); // TODO: pmedata is always a single element list, refactor
        pmedata = nullptr;
    }

    /* Free GPU memory and context */
    free_gpu_resources(fr, cr, shortRangedDeviceInfo);

    if (doMembed)
    {
        free_membed(membed);
    }

    gmx_hardware_info_free(hwinfo);

    /* Does what it says */
    print_date_and_time(fplog, cr->nodeid, "Finished mdrun", gmx_gettime());
    walltime_accounting_destroy(walltime_accounting);

    /* Close logfile already here if we were appending to it */
    if (MASTER(cr) && Flags.test(appendFiles))
    {
        gmx_log_close(fplog);
    }

    rc = (int)gmx_get_stop_condition();

#if GMX_THREAD_MPI
    /* we need to join all threads. The sub-threads join when they
       exit this function, but the master thread needs to be told to
       wait for that. */
    if (PAR(cr) && MASTER(cr))
    {
        tMPI_Finalize();
    }
#endif

    return rc;
}

Mdrunner::Mdrunner()
{
    restraintManager_ = ::gmx::restraint::Manager::instance();

    cr = init_commrec();
    // oenv initialized by parse_commond_args

    // dd_rank_order set according to argument processing logic (e.g. int(1))
    dd_rank_order = 1;

    // handleRestart sets &bDoAppendFiles, &bStartFromCpt

    // Flags set with lots of processing
    // Note: We cannot extract e.g. opt2parg_bSet("-append", asize(pa), pa) from this block.
    Flags.set(rerun, false);
    Flags.set(ddBondCheck, true);
    Flags.set(ddBondComm, true);
    Flags.set(tunePME, true);
    Flags.set(confOut, true);
    Flags.set(rerunVSite, false);
    Flags.set(reproducible, false);
    Flags.set(appendFiles, false);
    //Flags = Flags | (opt2parg_bSet("-append", asize(pa), pa) ? MD_APPENDFILESSET : 0);
    Flags.set(appendFilesSet, false);
    Flags.set(keepAndNumCpt, false);
    Flags.set(startFromCpt, false);
    Flags.set(resetCountersHalfWay, false);
    //Flags = Flags | (opt2parg_bSet("-ntomp", asize(pa), pa) ? MD_NTOMPSET : 0);
    Flags.set(ntompSet, false);
    Flags.set(imdWait, false);
    Flags.set(imdTerm, false);
    Flags.set(imdPull, false);

    // log opened to fplog if MASTER(cr) && !bDoAppendFiles

    // dddlb_opt set from processed options (e.g. const char* "auto")
    dddlb_opt = "auto";
    // nbpu_opt set from processed options (e.g. const char* "auto")
    nbpu_opt = "auto";
};

Mdrunner::~Mdrunner()
{
    /* Log file has to be closed in mdrunner if we are appending to it
       (fplog not set here) */
    // assert(cr != nullptr); // Todo: can we just initialize the cr in the constructor and keep it initialized?
    if (cr != nullptr && MASTER(cr) && !(Flags.test(appendFiles)))
    {
        gmx_log_close(fplog);
    }
    sfree(cr);
}

void Mdrunner::setTpx(std::shared_ptr<gmx::TpxState> newState)
{
    if (newState->isDirty())
    {
        GMX_THROW(gmx::InvalidInputError("Attempting to assign from a dirty state."));
    }
    // No good way to lock with default constructor and default moves for Mdrunner.
    // std::lock_guard<std::mutex> lock(stateAccess);
    // Todo: thread-safety
    // Locking the gmx::Mdrunner to serialize state updates would be nice, but
    // it would be sufficient to guarantee that the gmx::Mdrunner is thread-local
    //assert(tpxState_ != nullptr); // It is probably preferable to be able to assume tpxState_ is valid even if empty.
    if (tpxState_ != nullptr && tpxState_->isDirty())
    {
        // Calling code has a logic error: the old state is in use somewhere.
        GMX_THROW(gmx::APIError("Attempting to replace a state that may be in use (isDirty() == true)"));
    }
    tpxState_ = std::move(newState);
}

void Mdrunner::addPullPotential(std::shared_ptr<gmx::IRestraintPotential> puller,
                                std::string name)
{
    assert(restraintManager_ != nullptr);
    std::cout << "Registering restraint named " << name << std::endl;

    // When multiple restraints are used, it may be wasteful to register them separately.
    // Maybe instead register a Restraint Manager as a force provider.
    restraintManager_->addToSpec(std::move(puller),
                                 std::move(name));
}

void Mdrunner::declareFinalStep()
{
    simulationSignals_[eglsSTOPCOND].sig = true;
}

SimulationSignals *Mdrunner::signals() const
{
    return &simulationSignals_;
}

Mdrunner &Mdrunner::operator=(Mdrunner &&) = default;

Mdrunner::Mdrunner(Mdrunner &&) = default;

} // namespace gmx

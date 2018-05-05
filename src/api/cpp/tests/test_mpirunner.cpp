//
// Created by Eric Irrgang on 5/4/18.
//

#include "testingconfiguration.h"
//
//#include <memory>
//
#include "gmxapi/context.h"
//#include "gmxapi/md/mdmodule.h"
//#include "gmxapi/md.h"
#include "gmxapi/session.h"
#include "gmxapi/status.h"
#include "gmxapi/system.h"
//
//#include "gromacs/compat/make_unique.h"
//#include "gromacs/math/vectypes.h"
//#include "gromacs/mdtypes/iforceprovider.h"
//#include "gromacs/mdtypes/imdmodule.h"
//#include "gromacs/mdtypes/imdpoptionprovider.h"
//#include "gromacs/mdtypes/imdoutputprovider.h"
//#include "gromacs/mdtypes/tpxstate.h"
//#include "gromacs/restraint/restraintpotential.h"
//#include "gromacs/utility/arrayref.h"
#include "gromacs/utility/gmxmpi.h"

#include <gtest/gtest.h>

namespace
{

const auto filename = gmxapi::testing::sample_tprfilename;

//class DummyMDModule final : public gmx::IMDModule
//{
//    private:
//        class OptionProvider : public gmx::IMdpOptionProvider
//        {};
//        std::shared_ptr<OptionProvider> optionprovider{std::make_shared<OptionProvider>()};
//
//        class OutputProvider : public gmx::IMDOutputProvider
//        {};
//        std::shared_ptr<OutputProvider> outputprovider{std::make_shared<OutputProvider>()};
//
//        class ForceProvider : public gmx::IForceProvider
//        {
//            public:
//                void calculateForces(const t_commrec *cr,
//                                     const t_mdatoms *mdatoms,
//                                     const matrix box,
//                                     double t,
//                                     const rvec *x,
//                                     gmx::ArrayRef<gmx::RVec> force)
//                override
//                {
//                    force_called++;
//                };
//
//                unsigned int force_called{0};
//        };
//        std::shared_ptr<ForceProvider> forceprovider{std::make_shared<ForceProvider>()};
//
//        gmx::IForceProvider* getForceProvider()
//        {
//            return forceprovider.get();
//        };
//    public:
//        gmx::IMdpOptionProvider *mdpOptionProvider() override
//        {
//            return optionprovider.get();
//        }
//
//        gmx::IMDOutputProvider *outputProvider() override
//        {
//            return outputprovider.get();
//        }
//
//        void initForceProviders(ForceProviders *forceProviders) override
//        {
//            forceProviders->addForceProvider(getForceProvider());
//        }
//
//        unsigned int force_called() { return forceprovider->force_called; };
//};

TEST(MpiApiRunner, BasicMD)
{
    // Initialize an MPI environment. Launch GROMACS. When GROMACS is launched, gmx_init() should be called to initialize
    // MPI, recognize that MPI is already initialized, and increment its MPI recursion counter. Subsequent gmx operations
    // requiring MPI to be initialized should succeed.

    int isInitialized{0};
    bool needToFinalize{false};
    MPI_Initialized(&isInitialized);
    if (!isInitialized)
    {
        // GROMACS still supports MPI 1, which allows implementations to require passing argc and argv.
        MPI_Init(nullptr,nullptr);
        needToFinalize = true;
    }

    auto system = gmxapi::fromTprFile(filename);

    {
        std::shared_ptr<gmxapi::Context> context = gmxapi::defaultContext();
        ASSERT_TRUE(context != nullptr);
        ASSERT_TRUE(system != nullptr);
        auto session = system->launch();
        ASSERT_TRUE(session != nullptr);
        gmxapi::Status status;
        ASSERT_NO_THROW(status = session->run());
//        ASSERT_NO_THROW(session->run(1000));
        ASSERT_TRUE(status.success());
    }

    if (needToFinalize)
    {
        MPI_Finalize();
    }
}

} // end anonymous namespace

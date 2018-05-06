//
// Created by Eric Irrgang on 11/29/17.
//

#include "gmxapi/session.h"

#include <cassert>
#include "gromacs/mdtypes/tpxstate.h"
#include "gromacs/utility/basenetwork.h"
#include "gromacs/utility/init.h"
#include "gmxapi/md/mdmodule.h"
#include "gromacs/compat/make_unique.h"

#include "gmxapi/context.h"
#include "gmxapi/status.h"

#include "context-impl.h"
#include "workflow.h"
#include "session-impl.h"

namespace gmxapi
{

class MpiContextManager
{
    public:
        MpiContextManager()
        {
            gmx::init(nullptr, nullptr);
            assert(!gmxHasMPI() || gmx_mpi_initialized());
        };

        ~MpiContextManager()
        {
            gmx::finalize();
        }

        // Disallow copying
        MpiContextManager(const MpiContextManager&) = delete;
        MpiContextManager& operator=(const MpiContextManager&) = delete;

        // Trivial move
        MpiContextManager(MpiContextManager&&) noexcept = default;
        MpiContextManager& operator=(MpiContextManager&&) noexcept = default;
};

/*!
 * \brief Check if a an object can be considered "open".
 *
 * This should be generalized to an API idiom.
 *
 * \tparam T type that can be open or closed.
 * \param object something that has a concept of "open" or "closed."
 * \return true if open, false if closed, compiler error if non-sensical.
 */
template<class T>
bool isOpen(const T& object)
{
    return object.isOpen();
};

//{
//    (void) object;
//    static_assert(false, "Compiler could not find open/close concept for the given object.");
//    return false;
//}

template<>
bool isOpen<SessionImpl>(const SessionImpl& object)
{
    return object.isOpen();
}

template<>
bool isOpen<Session>(const Session& object)
{
    return object.isOpen();
}

bool SessionImpl::isOpen() const noexcept
{
    return status_ != nullptr;
}

Status SessionImpl::status() const noexcept
{
    return *status_;
}

std::unique_ptr<Status> SessionImpl::close()
{
    std::unique_ptr<Status> status{nullptr};
    status.swap(status_);
    assert(status_ == nullptr);
    return status;
}

Status SessionImpl::run() noexcept
{
    // Status is failure until proven otherwise.
    Status status{false};
    assert(runner_ != nullptr);
    auto rc = runner_->mdrunner();
    if (rc == 0)
    {
        status = true;
    }
    return status;
}

std::unique_ptr<SessionImpl> SessionImpl::create(std::shared_ptr<ContextImpl> context,
                                                 const Workflow &work)
{
    std::unique_ptr<SessionImpl> impl{new SessionImpl(std::move(context), work)};
    return impl;
}

SessionImpl::SessionImpl(std::shared_ptr<ContextImpl> context,
                         const Workflow &work) :
    status_{gmx::compat::make_unique<Status>(true)},
    context_{std::make_shared<Context>(std::move(context))},
    mpiContextManager_{gmx::compat::make_unique<MpiContextManager>()},
    runner_{nullptr}
{
    // mpiContextManager needs to be initialized before the gmx::Mdrunner constructor is called.

    // This is the libgromacs client code.
    // A Context should manage things like an MPI environment for the duration of the calling script.
    // A local Session should probably initialize and deinitialize libgromacs once and only once per instance.
    // We haven't yet considered a scenario with multiple Sessions per Context lifetime.

    // Check workflow spec, build graph for current context, launch and return new session.

    auto mdNode = work.getNode("MD");
    std::string filename{};
    if (mdNode != nullptr)
    {
        filename = mdNode->params();
    }
    auto newMdRunner = gmx::compat::make_unique<gmx::Mdrunner>();
    if (!filename.empty())
    {
        auto tpxState = gmx::TpxState::initializeFromFile(filename);
        newMdRunner->setTpx(std::move(tpxState));
        newMdRunner->initFromAPI(context_->MDArgs());
        runner_ = std::move(newMdRunner);
    }

    assert(status_ != nullptr);
    assert(context_ != nullptr);
    assert(mpiContextManager_ != nullptr);
    assert(runner_ != nullptr);
}

Status SessionImpl::setRestraint(std::shared_ptr<gmxapi::MDModule> module)
{
    assert(runner_ != nullptr);
    Status status{false};
    if (module != nullptr)
    {
        auto restraint = module->getRestraint();
        if (restraint != nullptr)
        {
            runner_->addPullPotential(restraint, module->name());
            status = true;
        }
    }
    return status;
}


Session::Session(std::unique_ptr<SessionImpl>&& impl) noexcept :
    impl_{std::move(impl)}
{
    assert(impl_ != nullptr);
    assert(impl == nullptr);
    assert(impl_->isOpen());
}

Status Session::run() noexcept
{
    assert(impl_ != nullptr);

    Status status{impl_->run()};
    return status;
}

Status Session::close()
{
    assert(impl_ != nullptr);

    Status status{false};
    if (isOpen())
    {
        // \todo catch exceptions when we know what they might be
        auto status_ptr = impl_->close();
        if (status_ptr != nullptr)
        {
            status = *status_ptr;
        }
        // what to do if we get nullptr?
    }

    return status;
}

Session::~Session()
{
    assert(impl_ != nullptr);
    if (isOpen())
    {
        try
        {
            impl_->close();
        }
        catch (const std::exception&)
        {
            // \todo find some exception-safe things to do with this via the Context interface.
        }
    }

}

bool Session::isOpen() const noexcept
{
    assert(impl_ != nullptr);
    const auto result = impl_->isOpen();
    return result;
}

Status setSessionRestraint(Session *session,
                           std::shared_ptr<gmxapi::MDModule> module)
{

    auto status = session->impl_->setRestraint(std::move(module));
    return status;
}

std::shared_ptr<Session> launchSession(Context* context, const Workflow& work) noexcept
{
    auto session = context->launch(work);
    return session;
}

SessionImpl::~SessionImpl() = default;

} // end namespace gmxapi

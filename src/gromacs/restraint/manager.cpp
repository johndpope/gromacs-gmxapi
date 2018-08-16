//
// Created by Eric Irrgang on 10/25/17.
//

/*! \internal
 * \brief Implement the restraint manager.
 *
 * \ingroup module_restraint
 */

#include "manager.h"

#include <cassert>
#include <memory>
#include <map>
#include <iostream>
#include <mutex>

#include "gromacs/compat/make_unique.h"
#include "gromacs/utility/exceptions.h"

namespace gmx
{

namespace restraint
{

// Initialize static members
std::shared_ptr<Manager> Manager::instance_ {
    nullptr
};
std::mutex Manager::initializationMutex_ {};


/*!
 * \brief Implementation class for restraint manager.
 */
class ManagerImpl
{
    public:

        void add(std::shared_ptr<::gmx::IRestraintPotential> restraint, std::string name);

        mutable std::vector < std::shared_ptr < ::gmx::IRestraintPotential>> restraint_;
};

void ManagerImpl::add(std::shared_ptr<::gmx::IRestraintPotential> restraint, std::string name)
{
    (void)name;
    restraint_.emplace_back(std::move(restraint));
}


Manager::Manager() : impl_(gmx::compat::make_unique<ManagerImpl>()) {};

Manager::~Manager() = default;

void Manager::clear() noexcept
{
    auto new_impl = gmx::compat::make_unique<ManagerImpl>();
    if (new_impl)
    {
        impl_.swap(new_impl);
    }
}

std::shared_ptr<Manager> Manager::instance()
{
    std::lock_guard<std::mutex> lock(initializationMutex_);
    if (instance_ == nullptr)
    {
        // What do we want to do if `new` throws?
        instance_ = std::shared_ptr<Manager>(new Manager);
    }
    assert(instance_ != nullptr);
    return instance_;
}


void Manager::addToSpec(std::shared_ptr<gmx::IRestraintPotential> puller,
                        std::string                               name)
{
    assert(impl_ != nullptr);
    impl_->add(std::move(puller), name);
}

std::vector < std::shared_ptr < IRestraintPotential>> Manager::getSpec() const
{
    if (!impl_)
    {
        GMX_THROW(InternalError("You found a bug!!!\n"
                                "gmx::restraint::Manager::getSpec should not be called before initializing manager!"));
    }
    return impl_->restraint_;
}

unsigned long Manager::countRestraints() noexcept
{
    return impl_->restraint_.size();
}

} // end namespace restraint
} // end namespace gmx

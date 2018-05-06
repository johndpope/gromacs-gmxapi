//
// Created by Eric Irrgang on 5/6/18.
//

// Test API context for GROMACS built with thread MPI.

#include "testingconfiguration.h"

#include "gmxapi/context.h"

#include <gtest/gtest.h>

namespace {

TEST(ApiContext, Basic)
{
    ASSERT_TRUE(gmxapi::Context::hasMPI() == false);
};

} // end anonymous namespace

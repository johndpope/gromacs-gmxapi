# For an executable target already defined, CMake tests are added with the
# following syntax:
#
# add_test(NAME <name> [CONFIGURATIONS [Debug|Release|...]]
#            [WORKING_DIRECTORY dir]
#            COMMAND <command> [arg1 [arg2 ...]])
#
# The Gromacs convention to get the appropriate compilation environment for
# unit tests is the gmx_add_unit_test macro, defined in
# src/testutils/TestMacros.cmake, which does not allow arbitrary argument
# passing to add_test. Instead, use set_tests_properties().

gmx_add_gtest_executable(
    gmxapi-test
    test_version_helpers.cpp
    )

target_include_directories(gmxapi-test PRIVATE
                           ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(gmxapi-test Gromacs::gmxapi)

# Unit tests for implementation details may require access to private headers.
#target_link_libraries(gmxapi-impl-test Gromacs::gmxapi Gromacs::gmxapiImpl)

gmx_register_gtest_test(GmxapiExternalInterfaceTests gmxapi-test INTEGRATION_TEST)

set_tests_properties(GmxapiExternalInterfaceTests PROPERTIES
                     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

gmx_add_gtest_executable(
    gmxapi-mpi-test MPI
    test_version_helpers.cpp
    )

target_include_directories(gmxapi-mpi-test PRIVATE
                           ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(gmxapi-mpi-test Gromacs::gmxapi)

# We don't yet have anything that can understand the ``-ntmpi`` command-line
# option that will be passed to a THREAD_MPI version of a test that specifies MPI_RANKS
if(NOT GMX_THREAD_MPI)
    gmx_register_gtest_test(GmxapiMpiTests gmxapi-mpi-test MPI_RANKS 2 INTEGRATION_TEST)

    if(BUILD_TESTING)
        # Apparently gmx_register_gtest_test has some conditional like the following
        # when MPI_RANKS is set or when gmx_add_gtest_executable has MPI set.
        if(GMX_MPI OR GMX_THREAD_MPI)
            set_tests_properties(GmxapiMpiTests PROPERTIES
                                 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
        endif(GMX_MPI OR GMX_THREAD_MPI)
    endif()
endif()

# If we want to override the API version in the version.h for testing purposes, we'll do it another way...
#target_compile_definitions(gmxapi-test PUBLIC
#    GMXAPI_MAJOR=${GMXAPI_MAJOR}
#    GMXAPI_MINOR=${GMXAPI_MINOR}
#    GMXAPI_PATCH=${GMXAPI_PATCH})

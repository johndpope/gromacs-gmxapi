# The include directory should be mostly empty so that we can use it internally as
# the public include directory during build and testing. No targets are defined.
# Linking targets in the build tree should use the gmxapi INTERFACE. Scripts
# Needing access to a list of pre-installed public headers can look at the
# GMXAPI_PUBLIC_BUILD_HEADERS variable in the src/api scope (after the src/api/cpp
# subdirectory has been processed).

include(cmake/CPM.cmake)

CPMAddPackage(
        NAME Boost
        VERSION 1.92.0 # Versions less than 1.85.0 may need patches for installation targets.
        URL https://github.com/boostorg/boost/releases/download/boost-1.92.0/boost-1.92.0-cmake.tar.xz
        URL_HASH SHA256=9bed76128d4e46755dbe818487788c6fceb6f72b378f4daa49b7e1e600d9088d
        OPTIONS "BOOST_ENABLE_CMAKE ON" "BOOST_SKIP_INSTALL_RULES ON" # Set `OFF` for installation
        "BUILD_SHARED_LIBS OFF" "BOOST_INCLUDE_LIBRARIES beast\\\;asio\\\;unordered\\\;leaf" # Note the escapes!
)
CPMAddPackage(
        NAME highway
        GITHUB_REPOSITORY google/highway
        GIT_TAG 1.4.0
        OPTIONS
        "HWY_ENABLE_TESTS OFF"
        "HWY_ENABLE_EXAMPLES OFF"
        "HWY_ENABLE_CONTRIB ON"
)
CPMAddPackage("gh:stephenberry/glaze@9.0.0")
CPMAddPackage("gh:google/googletest@1.18.0")
CPMAddPackage("gh:google/benchmark@1.9.5")
CPMAddPackage("gh:odygrd/quill@13.0.0")
CPMAddPackage("gh:fmtlib/fmt#12.2.0")
CPMAddPackage("gh:tcbrindle/flux@0.4.0")
CPMAddPackage("gh:microsoft/mimalloc@3.5.4")

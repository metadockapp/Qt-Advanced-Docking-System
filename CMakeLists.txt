cmake_minimum_required(VERSION 3.16)

# Generate compile_commands.json.
#
# This is primarily useful for development tools such as clangd,
# clang-tidy, and other static-analysis tools.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable the MSVC runtime library selection abstraction introduced with
# CMake 3.15. This allows CMAKE_MSVC_RUNTIME_LIBRARY and the
# MSVC_RUNTIME_LIBRARY target property to be used consistently.
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

# ---------------------------------------------------------------------------
# CMake modules
# ---------------------------------------------------------------------------

# Add the project's custom CMake modules, e.g. Versioning.cmake, to the
# module search path.
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

# ---------------------------------------------------------------------------
# Project
# ---------------------------------------------------------------------------

project(
    QtADS
    VERSION 5.1.1
    LANGUAGES CXX
)

# Determine whether QtADS is being built as the top-level project.
#
# PROJECT_IS_TOP_LEVEL was introduced with CMake 3.21. For older CMake
# versions, determine the state manually.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
    set(QtADS_IS_TOP_LEVEL "${PROJECT_IS_TOP_LEVEL}")
else()
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        set(QtADS_IS_TOP_LEVEL TRUE)
    else()
        set(QtADS_IS_TOP_LEVEL FALSE)
    endif()
endif()

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------

# Build QtADS as a static library instead of a shared library.
option(BUILD_STATIC "Build the static library" OFF)

# Build the example applications and demo.
option(BUILD_EXAMPLES "Build the examples" ON)

# ---------------------------------------------------------------------------
# Platform directory
# ---------------------------------------------------------------------------

# MetaDock fork: no platform subdirectory by default, so the library lands
# directly in the CMake output directory. ADS_PLATFORM_DIR still overrides it.
if(ADS_PLATFORM_DIR)
    set(ads_PlatformDir "${ADS_PLATFORM_DIR}")
else()
    set(ads_PlatformDir "")
endif()

# ---------------------------------------------------------------------------
# CMake policies
# ---------------------------------------------------------------------------

if(QtADS_IS_TOP_LEVEL)
    # Enable folder organization by default for generators supporting it.
    # CMP0143 was introduced with CMake 3.26.
    if(POLICY CMP0143)
        cmake_policy(SET CMP0143 NEW)
    endif()

    # Enable parallel compilation when building with MSVC.
    if(MSVC)
        add_compile_options(/MP)
    endif()
endif()


# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------

add_subdirectory(src)

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
    add_subdirectory(demo)
endif()

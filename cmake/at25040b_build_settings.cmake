#===============================================================================
# AT25040B driver: build settings
#===============================================================================
# Read by the root CMakeLists.txt in both of its modes (plain CMake and the
# ESP-IDF component): target name, version, include directories, sources and
# ESP-IDF requirements. This file is the single source
# of the version; tests/CMakeLists.txt checks every other copy against it.
#===============================================================================

include_guard(GLOBAL)

set(AT25040B_TARGET_NAME "at25040b")

#===============================================================================
# Version (single source of truth)
#===============================================================================
set(AT25040B_VERSION_MAJOR 0)
set(AT25040B_VERSION_MINOR 1)
set(AT25040B_VERSION_PATCH 0)
set(AT25040B_VERSION "${AT25040B_VERSION_MAJOR}.${AT25040B_VERSION_MINOR}.${AT25040B_VERSION_PATCH}")

#===============================================================================
# Generated version header (build tree, never the source tree)
#===============================================================================
set(AT25040B_VERSION_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/../inc/at25040b_version.h.in")
set(AT25040B_VERSION_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/at25040b_generated")
set(AT25040B_VERSION_HEADER "${AT25040B_VERSION_HEADER_DIR}/at25040b_version.h")

# The directory must exist before ESP-IDF's idf_component_register() checks the include paths.
file(MAKE_DIRECTORY "${AT25040B_VERSION_HEADER_DIR}")
configure_file("${AT25040B_VERSION_TEMPLATE}" "${AT25040B_VERSION_HEADER}" @ONLY)

#===============================================================================
# Public include directories, sources, ESP-IDF requirements
#===============================================================================
set(AT25040B_PUBLIC_INCLUDE_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../inc"
    "${AT25040B_VERSION_HEADER_DIR}"
)

# Header-only: the template implementation is src/at25040b.ipp, included by inc/at25040b.hpp.
set(AT25040B_SOURCE_FILES)

set(AT25040B_IDF_REQUIRES)

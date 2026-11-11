#===============================================================================
# AT25040B driver: root CMake
#===============================================================================
# One file, two build systems:
#
# - ESP-IDF (ESP_PLATFORM): this folder is an ESP-IDF component. Point
#   EXTRA_COMPONENT_DIRS at it, check it out under components/, or add it as a
#   component-manager git dependency. menuconfig reads _config/Kconfig.
# - Any other CMake build (host, STM32, RP2040, nRF, ...): one INTERFACE target,
#   at25040b, consumed as at25040b::at25040b both through add_subdirectory() and
#   through find_package(at25040b) after install.
#
#   cmake -S . -B build -DAT25040B_BUILD_TESTS=ON
#   cmake --build build && ctest --test-dir build --output-on-failure
#===============================================================================
cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/at25040b_build_settings.cmake)

#===============================================================================
# ESP-IDF component
#===============================================================================
if(ESP_PLATFORM)
    idf_component_register(
        SRCS ${AT25040B_SOURCE_FILES}
        INCLUDE_DIRS ${AT25040B_PUBLIC_INCLUDE_DIRS}
        REQUIRES ${AT25040B_IDF_REQUIRES}
        KCONFIG ${CMAKE_CURRENT_LIST_DIR}/_config/Kconfig
    )
    if(AT25040B_SOURCE_FILES)
        target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
    else()
        target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_20)
    endif()
    return()
endif()

project(at25040b_driver
    VERSION ${AT25040B_VERSION}
    DESCRIPTION "C++20 driver for the Microchip AT25010B, AT25020B and AT25040B SPI serial EEPROMs"
    LANGUAGES CXX)

include(GNUInstallDirs)

# ISO C++20 for every target this project builds (tests, fuzzers, examples), never the GNU dialect
# that CMake picks by default (handbook KB-CPP-001, KB-CPP20-001). CXX_EXTENSIONS does not travel
# through an INTERFACE target, so each folder that builds a target sets it as well.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The installed layout keeps the source tree's inc/ + src/ pair, because at25040b.hpp includes
# "../src/at25040b.ipp" relative to itself:
#   <prefix>/include/at25040b/inc/*.hpp, at25040b_version.h
#   <prefix>/include/at25040b/src/at25040b.ipp
set(AT25040B_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/at25040b/inc")

#===============================================================================
# Target
#===============================================================================
add_library(${AT25040B_TARGET_NAME} INTERFACE)
target_include_directories(${AT25040B_TARGET_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/inc>
        $<BUILD_INTERFACE:${AT25040B_VERSION_HEADER_DIR}>
        $<INSTALL_INTERFACE:${AT25040B_INSTALL_INCLUDEDIR}>
)
target_compile_features(${AT25040B_TARGET_NAME} INTERFACE cxx_std_20)

# at25040b::at25040b everywhere: the alias for add_subdirectory(), EXPORT_NAME for the installed package.
add_library(at25040b::at25040b ALIAS ${AT25040B_TARGET_NAME})
set_target_properties(${AT25040B_TARGET_NAME} PROPERTIES EXPORT_NAME at25040b)

option(AT25040B_ENABLE_WARNINGS "Add -Wall -Wextra -Wpedantic to every target that links at25040b::at25040b" OFF)
if(AT25040B_ENABLE_WARNINGS)
    target_compile_options(${AT25040B_TARGET_NAME} INTERFACE -Wall -Wextra -Wpedantic)
endif()

#===============================================================================
# Host tests, documentation snippets, install check (tests/)
#===============================================================================
option(AT25040B_BUILD_TESTS "Build the host tests in tests/" OFF)
option(AT25040B_BUILD_FUZZERS "Also build the libFuzzer targets in tests/fuzz/ (Clang only)" OFF)
if(AT25040B_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

#===============================================================================
# Install and package export
#===============================================================================
include(CMakePackageConfigHelpers)

install(TARGETS ${AT25040B_TARGET_NAME} EXPORT at25040bTargets
    INCLUDES DESTINATION ${AT25040B_INSTALL_INCLUDEDIR})

install(DIRECTORY inc/ DESTINATION ${AT25040B_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
install(FILES "${AT25040B_VERSION_HEADER}" DESTINATION ${AT25040B_INSTALL_INCLUDEDIR})
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/at25040b/src
    FILES_MATCHING PATTERN "*.ipp")

install(EXPORT at25040bTargets
    FILE at25040bTargets.cmake
    NAMESPACE at25040b::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/at25040b)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/at25040bConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/at25040bConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/at25040b)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/at25040bConfigVersion.cmake"
    VERSION ${AT25040B_VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/at25040bConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/at25040bConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/at25040b)

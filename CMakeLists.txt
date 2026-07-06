# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.20)

project(edn-cpp
  VERSION 0.1.0
  DESCRIPTION "edn-cpp — standalone C++20 EDN parser and emitter"
  HOMEPAGE_URL "https://github.com/nous/edn-cpp"
  LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Dependency policy (mirrors nous native convention)
#
# - No Boost. C++20 STL covers all use cases.
# - FetchContent + pinned tag for any dependency with a build step.
# - FETCHCONTENT_UPDATES_DISCONNECTED: skip network on subsequent invocations.
# - Tests use Catch2 v3 (Boost Software License, header-friendly).
# ---------------------------------------------------------------------------

include(FetchContent)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
include(cmake/Sanitizers.cmake)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL
    "Skip network fetch if already populated")

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------

option(EDN_CPP_BUILD_TESTS   "Build edn-cpp unit tests"   OFF)
option(EDN_CPP_BUILD_EXAMPLES "Build edn-cpp examples"    OFF)
option(EDN_CPP_INSTALL       "Generate install targets"   ON)

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------

add_library(edn-cpp
  src/intern.cpp
  src/value.cpp
  src/parser.cpp
  src/emitter.cpp
  src/builtins.cpp
)

add_library(edn::edn ALIAS edn-cpp)

target_include_directories(edn-cpp
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    src
)

nomos_sanitize_target(edn-cpp)

set_target_properties(edn-cpp PROPERTIES
  CXX_STANDARD          20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS        OFF
  VERSION               ${PROJECT_VERSION}
  SOVERSION             0
  EXPORT_NAME           edn
)

# ---------------------------------------------------------------------------
# Compiler warnings (opt-in; does not affect consumers)
# ---------------------------------------------------------------------------

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  target_compile_options(edn-cpp PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic -Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
  )
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

if(EDN_CPP_BUILD_TESTS)
  enable_testing()

  # Catch2 v3 — BSL-1.0, header-friendly, no extra dependencies.
  # Pin: v3.7.1 — update deliberately and document the reason.
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE)
  FetchContent_MakeAvailable(Catch2)

  add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Install + package export (FetchContent-friendly without this; install
# targets are for system-level or find_package consumers)
# ---------------------------------------------------------------------------

if(EDN_CPP_INSTALL)
  install(TARGETS edn-cpp
    EXPORT  edn-cpp-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )

  install(DIRECTORY include/edn
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(EXPORT edn-cpp-targets
    FILE        edn-cpp-targets.cmake
    NAMESPACE   edn::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edn-cpp
  )

  configure_package_config_file(
    cmake/edn-cpp-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/edn-cpp-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edn-cpp
  )

  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/edn-cpp-config-version.cmake
    VERSION       ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/edn-cpp-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/edn-cpp-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/edn-cpp
  )
endif()

# REUSE licence compliance target
find_program(REUSE_EXECUTABLE reuse)
if(REUSE_EXECUTABLE)
    add_custom_target(lint-reuse
        COMMAND ${REUSE_EXECUTABLE} lint
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Checking REUSE compliance"
        VERBATIM
    )
    add_test(NAME reuse-lint
        COMMAND ${REUSE_EXECUTABLE} lint
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

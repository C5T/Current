# TODO(dkorolev): Consider intelligently re-running `cmake` from the `Makefile` if the set of source files has changed.

cmake_minimum_required(VERSION 3.14.1)

project(cmake_trivial_2023 C CXX)

set (CMAKE_CXX_STANDARD 17)
find_package(Threads REQUIRED)

# Settings for `googletest`. It builds faste without `gmock`.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
option(BUILD_GMOCK OFF)

# The helper to clone a dependency, or use it from a sibling dir if available.
function(UseGitOrCloneImpl dep remote use_custom_branch custom_branch)
  if(EXISTS "${CMAKE_SOURCE_DIR}/${dep}" AND IS_DIRECTORY "${CMAKE_SOURCE_DIR}/${dep}")
    message(STATUS "Using `${dep}` from `CMAKE_SOURCE_DIR/${dep}'.")
    add_subdirectory("${CMAKE_SOURCE_DIR}/${dep}" ${dep})
    message(STATUS "Using `${dep}` from `CMAKE_SOURCE_DIR/${dep}': Configured.")
    set("SRC_DIR_${dep}" "${CMAKE_SOURCE_DIR}/${dep}" PARENT_SCOPE)
  elseif(EXISTS "${CMAKE_SOURCE_DIR}/../${dep}" AND IS_DIRECTORY "${CMAKE_SOURCE_DIR}/../${dep}")
    message(STATUS "Using `${dep}` from `CMAKE_SOURCE_DIR/../${dep}'.")
    add_subdirectory("${CMAKE_SOURCE_DIR}/../${dep}" ${dep})
    message(STATUS "Using `${dep}` from `CMAKE_SOURCE_DIR/../${dep}': Configured.")
    set("SRC_DIR_${dep}" "${CMAKE_SOURCE_DIR}/../${dep}" PARENT_SCOPE)
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../${dep}" AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../${dep}")
    message(STATUS "Using `${dep}` from `CMAKE_CURRENT_SOURCE_DIR/../${dep}'.")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../${dep}" ${dep})
    message(STATUS "Using `${dep}` from `CMAKE_CURRENT_SOURCE_DIR/../${dep}': Configured.")
    set("SRC_DIR_${dep}" "${CMAKE_CURRENT_SOURCE_DIR}/../${dep}" PARENT_SCOPE)
  else()
    if(NOT use_custom_branch)
      message(STATUS "Cloning `${dep}` from `${remote}` using the default branch ...")
      execute_process(OUTPUT_QUIET ERROR_QUIET COMMAND git clone --depth 1 ${remote})
    else()
      message(STATUS "Cloning `${dep}` from `${remote}:${branch}` ...")
      execute_process(OUTPUT_QUIET ERROR_QUIET COMMAND git clone --depth 1 -b ${custom_branch} ${remote})
    endif()
    add_subdirectory("${CMAKE_SOURCE_DIR}/${dep}" ${dep})
    message(STATUS "Cloning `${dep}` from `${remote}`: Configured.")
    file(TOUCH .gitignore)
    file(APPEND .gitignore "${dep}/\n")  # NOTE(dkorolev): This may add extra `.gitignore` lines, ignore for now.
    set("SRC_DIR_${dep}" "${CMAKE_SOURCE_DIR}/${dep}" PARENT_SCOPE)
  endif()
endfunction()

function(UseGitOrClone dep remote)
  UseGitOrCloneImpl("${dep}" "${remote}" FALSE _)
endfunction()

function(UseGitOrCloneBranch dep remote branch)
  UseGitOrCloneImpl("${dep}" "${remote}" TRUE "${branch}")
endfunction()

# NOTE(dkorolev): This is about to be changed back once https://github.com/C5T/Current/pull/966 is merged!
UseGitOrCloneBranch(current https://github.com/dkorolev/current current_build_autogen_header_for_cmake)
UseGitOrClone(googletest https://github.com/c5t/googletest)

add_custom_target(C5T_CURRENT_BUILD_H_TARGET ALL DEPENDS "${SRC_DIR_current}/current_build.h")

add_custom_command(OUTPUT "${SRC_DIR_current}/current_build.h"
                   COMMAND "${SRC_DIR_current}/scripts/gen-current-build.sh"
                   ARGS "${SRC_DIR_current}/current_build.h"
                   DEPENDS src)

set(C5T_LIBRARIES "Threads::Threads" "C5T")

# Declare shared libraries as shared library targets. Do not link them against anything external.
file(GLOB_RECURSE BINARY_SOURCE_FILES "src/dlib_*.cc")
foreach(SHARED_LIBRARY_SOURCE_FILE ${BINARY_SOURCE_FILES})
  get_filename_component(SHARED_LIBRARY_TARGET_NAME "${SHARED_LIBRARY_SOURCE_FILE}" NAME_WE)
  add_library(${SHARED_LIBRARY_TARGET_NAME} SHARED "${SHARED_LIBRARY_SOURCE_FILE}") 
  # TODO(dkorolev): Might be worth it to `grep` this `dlib_*.cc` source file for required library dependencies.
  target_compile_definitions(${SHARED_LIBRARY_TARGET_NAME} PRIVATE C5T_CMAKE_PROJECT)
  target_link_libraries(${SHARED_LIBRARY_TARGET_NAME} PRIVATE "${C5T_LIBRARIES}")
endforeach()

set(ALL_LIBRARIES "${C5T_LIBRARIES}")
# Declare libraries as library targets. And add them into the `ALL_LIBRARIES` list.
file(GLOB_RECURSE LIBRARY_SOURCE_FILES "src/lib_*.cc")
foreach(LIBRARY_SOURCE_FILE ${LIBRARY_SOURCE_FILES})
  get_filename_component(LIBRARY_TARGET_NAME "${LIBRARY_SOURCE_FILE}" NAME_WE)
  add_library(${LIBRARY_TARGET_NAME} "${LIBRARY_SOURCE_FILE}")
  target_compile_definitions(${LIBRARY_TARGET_NAME} PRIVATE C5T_CMAKE_PROJECT)
  target_link_libraries(${LIBRARY_TARGET_NAME} PRIVATE "${C5T_LIBRARIES}")
  list(APPEND ALL_LIBRARIES "${LIBRARY_TARGET_NAME}")
endforeach()

# Declare binaries as binary targets. And link them against all the libraries.
file(GLOB_RECURSE BINARY_SOURCE_FILES "src/*.cc")
foreach(BINARY_SOURCE_FILE ${BINARY_SOURCE_FILES})
  get_filename_component(BINARY_TARGET_NAME "${BINARY_SOURCE_FILE}" NAME_WE)
  if(NOT (BINARY_TARGET_NAME MATCHES "^lib_.*$" OR BINARY_TARGET_NAME MATCHES "^test_.*$" OR BINARY_TARGET_NAME MATCHES "^dlib_.*$"))
    add_executable(${BINARY_TARGET_NAME} "${BINARY_SOURCE_FILE}") 
    target_compile_definitions(${BINARY_TARGET_NAME} PRIVATE C5T_CMAKE_PROJECT)
    target_link_libraries(${BINARY_TARGET_NAME} PRIVATE "${ALL_LIBRARIES}")
  endif()
endforeach()

# Declare tests as test targets. And link them against all the libraries.
enable_testing()
file(GLOB_RECURSE TEST_SOURCE_FILES "src/test_*.cc")
foreach(TEST_SOURCE_FILE ${TEST_SOURCE_FILES})
  get_filename_component(TEST_TARGET_NAME "${TEST_SOURCE_FILE}" NAME_WE)
  add_executable(${TEST_TARGET_NAME} "${TEST_SOURCE_FILE}") 
  target_link_libraries(${TEST_TARGET_NAME} PRIVATE gtest_main "${ALL_LIBRARIES}")
  add_test(NAME ${TEST_TARGET_NAME} COMMAND ${TEST_TARGET_NAME})
endforeach()

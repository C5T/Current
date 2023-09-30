# TODO(dkorolev): Use non-header-only `googletest`.'

cmake_minimum_required(VERSION 3.14.1)

project(C5T C CXX)

message(STATUS "Add `C5T` into `target_link_libraries()` of your `CMakeLists.txt` to use `current` headers.")

set (CMAKE_CXX_STANDARD 17)
find_package(Threads REQUIRED)

add_library(C5T INTERFACE)
target_include_directories(C5T INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

# Generate `current_build.h`. It is needed for the `karl/` test.
# NOTE(dkorolev): It's fne that it will be in the `scripts/` directory for now.
execute_process(COMMAND make -f MakefileWithCurrentBuild phony_current_build
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/scripts"
                OUTPUT_QUIET)

# Copy all the `golden/*` files for all the tests under a single `golden/` directory for the tests to pass.
# NOTE(dkorolev): This is how our non-`cmake` test runner operates.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/golden")
file(GLOB_RECURSE GOLDEN_FILES "*/golden/*")
foreach(GOLDEN_FILE ${GOLDEN_FILES})
  execute_process(COMMAND cp "${GOLDEN_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/golden")
endforeach()

enable_testing()
file(GLOB_RECURSE TEST_SOURCE_FILES_FULL_PATHS "test.cc")
foreach(TEST_SOURCE_FULL_NAME_CC ${TEST_SOURCE_FILES_FULL_PATHS})
  file(RELATIVE_PATH TEST_SOURCE_RELATIVE_NAME_CC ${CMAKE_CURRENT_SOURCE_DIR} ${TEST_SOURCE_FULL_NAME_CC})
  string(REPLACE "/" "_" TEST_NAME_CC "${TEST_SOURCE_RELATIVE_NAME_CC}")
  string(REGEX REPLACE "\.cc$" "" TEST_NAME "${TEST_NAME_CC}")
  add_executable(${TEST_NAME} "${TEST_SOURCE_FULL_NAME_CC}") 
  target_compile_definitions(${TEST_NAME} PRIVATE CURRENT_CMAKE_BUILT_TEST)
  target_include_directories(${TEST_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/scripts")  # For `current_build.h`, needed for `karl/`.
  target_link_libraries(${TEST_NAME} PRIVATE Threads::Threads dl C5T)
  add_test(${TEST_NAME} ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

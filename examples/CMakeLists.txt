# Note: This file is included via add_subdirectory() from the root CMake.
# Keep it minimal: no project() or policies.

# We do NOT build example binaries. We only create object targets so
# CMake can emit compile commands for clang tooling.

# Common include dirs for examples (inherit public ones from AsyncGSM; add mocks)

# --- Unity ---
 FetchContent_Declare(
   Unity
   URL https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v2.6.0.zip
)
FetchContent_MakeAvailable(Unity)

file(GLOB_RECURSE EXAMPLES_ALL_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_LIST_DIR}/*.c
  ${CMAKE_CURRENT_LIST_DIR}/*.cpp
)
if(EXAMPLES_ALL_SOURCES)
  list(FILTER EXAMPLES_ALL_SOURCES EXCLUDE REGEX "/\\.pio/")
endif()

# Single OBJECT target for all examples (indexing-only)
add_library(examples_index_objs OBJECT ${EXAMPLES_ALL_SOURCES})
set_target_properties(examples_index_objs PROPERTIES EXCLUDE_FROM_ALL ON)
target_link_libraries(examples_index_objs
  PRIVATE
  AsyncGSM
  unity
)

target_include_directories(examples_index_objs PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../src
  ${asyncathandler_SOURCE_DIR}/test/mocks
  ${gmock_SOURCE_DIR}/include
  ${gtest_SOURCE_DIR}/include
  ${Unity_SOURCE_DIR}/src
  ${asyncathandler_SOURCE_DIR}/test/mocks/freertos
)

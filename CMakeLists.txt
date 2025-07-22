cmake_minimum_required(VERSION 3.15)
project(AsyncGSM CXX)

set(CMAKE_CXX_STANDARD 17)

# Paths
set(MOCK_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test/mocks)
set(LIB_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# === DEPENDENCIES ===
include(FetchContent)
include_directories(
  ${MOCK_INC_DIR}
  ${LIB_SRC_DIR}
)
cmake_policy(SET CMP0135 NEW) # This is fine here.

# --- AsyncATHandler ---
FetchContent_Declare(
  AsyncATHandler
  URL https://github.com/ByteNana/AsyncATHandler/archive/refs/heads/master.zip
  DOWNLOAD_EXTRACT_TIMESTAMP true
)
# Use MakeAvailable for AsyncATHandler to handle download and build,
# addressing the deprecation warning.
FetchContent_MakeAvailable(AsyncATHandler)

# --- GoogleTest/GoogleMock ---
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP true
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# --- Unity ---
FetchContent_Declare(
  Unity
  URL https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v2.6.0.zip
)
FetchContent_MakeAvailable(Unity)

# === YOUR LIBRARY ===

# Get your source files
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  ${LIB_SRC_DIR}/*.cpp
  ${LIB_SRC_DIR}/*.h
)

add_library(AsyncGSM STATIC ${SRC_FILES})

target_include_directories(AsyncGSM PUBLIC
  ${LIB_SRC_DIR}
  ${asyncathandler_SOURCE_DIR}/src
)

target_include_directories(AsyncGSM PRIVATE
  ${MOCK_INC_DIR} # Your Arduino mocks
  ${asyncathandler_SOURCE_DIR}/test/mocks
)


# Link with the built libraries
# This links the actual library files, but does NOT necessarily handle include paths
# for dependencies that don't correctly export their PUBLIC includes.
target_link_libraries(AsyncGSM PUBLIC
  AsyncATHandler # Link to the AsyncATHandler library target created by FetchContent_MakeAvailable
  gmock
  gtest
  Unity
)


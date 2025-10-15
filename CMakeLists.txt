cmake_minimum_required(VERSION 3.15)
project(AsyncGSM VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PROJECT_VERSION_SUFIX "-rc.1")
set(CMAKE_PROJECT_VERSION "${PROJECT_VERSION}${PROJECT_VERSION_SUFIX}")

# --- Set build type to Debug for debugging symbols ---
set(CMAKE_BUILD_TYPE Debug)

# Config flag to differentiate between embedded vs native build
option(NATIVE_BUILD "Use mock Arduino/FreeRTOS headers for native testing" ON)
option(ASYNCGSM_BUILD_TESTS "Build tests" ON)
option(ASYNCGSM_ENABLE_EXAMPLES_INDEXING "Include examples in build for compile_commands.json" ON)

# Paths
set(LIB_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)

# === NATIVE BUILD MODE ===
message(STATUS "🔧 Building with mock Arduino/FreeRTOS headers")

# === DEPENDENCIES ===
include(FetchContent)
cmake_policy(SET CMP0135 NEW)

# --- AsyncATHandler ---
# Disable AsyncATHandler's tests to avoid building them in AsyncGSM
set(ASYNCAT_HANDLER_BUILD_TESTS OFF CACHE BOOL "Disable AsyncATHandler tests" FORCE)

FetchContent_Declare(
  AsyncATHandler
  GIT_REPOSITORY https://github.com/ByteNana/AsyncATHandler
  GIT_TAG rc-0.1.1
)
FetchContent_MakeAvailable(AsyncATHandler)

# --- ArduinoHttpClient ---
FetchContent_Declare(
  ArduinoHttpClient
  GIT_REPOSITORY https://github.com/arduino-libraries/ArduinoHttpClient
  GIT_TAG master
)
FetchContent_MakeAvailable(ArduinoHttpClient)

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

# Link pthread for POSIX port
find_package(Threads REQUIRED)

# === INCLUDE DIRECTORIES ===
include_directories(
  ${LIB_SRC_DIR}
  ${arduinonativemocks_SOURCE_DIR}/src       # ArduinoHttpClient headers
  ${arduinohttpclient_SOURCE_DIR}/src       # ArduinoHttpClient headers
  ${gmock_SOURCE_DIR}/include
  ${gtest_SOURCE_DIR}/include
  ${Unity_SOURCE_DIR}/src
)

# === LIBRARY ===
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  ${LIB_SRC_DIR}/*.cpp
  ${LIB_SRC_DIR}/*.h
  ${LIB_SRC_DIR}/*.c
)

# Get ArduinoHttpClient source files
file(GLOB_RECURSE ARDUINO_HTTP_CLIENT_SOURCES CONFIGURE_DEPENDS
  ${arduinohttpclient_SOURCE_DIR}/src/*.cpp
  ${arduinohttpclient_SOURCE_DIR}/src/*.h
  ${arduinohttpclient_SOURCE_DIR}/src/*.c
)

add_library(AsyncGSM STATIC ${SRC_FILES} ${ARDUINO_HTTP_CLIENT_SOURCES})

# Add include directories for AsyncGSM
target_include_directories(AsyncGSM PUBLIC
  ${LIB_SRC_DIR}
  ${arduinonativemocks_SOURCE_DIR}/src
  ${asyncathandler_SOURCE_DIR}/src
  ${arduinohttpclient_SOURCE_DIR}/src
)

# Link FreeRTOS-Sim to your main library so all files can use it
target_link_libraries(AsyncGSM PUBLIC
  ArduinoNativeMocks
  AsyncATHandler
)
enable_language(C)

set(LOG_LEVEL "3" CACHE STRING "Log level (0-5, where 5 is most verbose)")
set_property(CACHE LOG_LEVEL PROPERTY STRINGS "0" "1" "2" "3" "4" "5")
# Set the log level for AsyncATHandler
target_compile_definitions(AsyncATHandler PUBLIC LOG_LEVEL=${LOG_LEVEL})

# === TESTS ===
if(ASYNCGSM_BUILD_TESTS)
  add_compile_definitions(LOG_LEVEL=${LOG_LEVEL})
  enable_testing()
  add_subdirectory(test)
endif()

if(ASYNCGSM_ENABLE_EXAMPLES_INDEXING)
  add_subdirectory(examples)
endif()

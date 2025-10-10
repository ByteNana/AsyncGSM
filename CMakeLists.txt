cmake_minimum_required(VERSION 3.15)
project(AsyncGSM C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Set build type to Debug for debugging symbols ---
set(CMAKE_BUILD_TYPE Debug)

# Config flag to differentiate between embedded vs native build
option(NATIVE_BUILD "Use mock Arduino/FreeRTOS headers for native testing" ON)
option(ASYNCGSM_BUILD_TESTS "Build tests" ON)
option(ASYNCGSM_ENABLE_EXAMPLES_INDEXING "Include examples in build for compile_commands.json" ON)

# Paths
set(LIB_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(MOCK_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test/mocks)
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)

# === NATIVE BUILD MODE ===
message(STATUS "🔧 Building with mock Arduino/FreeRTOS headers")

# === DEPENDENCIES ===
include(FetchContent)
cmake_policy(SET CMP0135 NEW)

# --- FreeRTOS Shim using AsyncATHandler mocks (no external FreeRTOS download) ---
find_package(Threads REQUIRED)

# --- AsyncATHandler ---
# Disable AsyncATHandler's tests to avoid building them in AsyncGSM
set(ASYNCAT_HANDLER_BUILD_TESTS OFF CACHE BOOL "Disable AsyncATHandler tests" FORCE)

FetchContent_Declare(
  AsyncATHandler
  GIT_REPOSITORY https://github.com/ByteNana/AsyncATHandler
  GIT_TAG rc-0.1.1
)
FetchContent_MakeAvailable(AsyncATHandler)

# Ensure FreeRTOS_Sim_Lib from AsyncATHandler includes required mock sources
if(TARGET FreeRTOS_Sim_Lib)
  target_sources(FreeRTOS_Sim_Lib PRIVATE
    ${asyncathandler_SOURCE_DIR}/test/mocks/freertos/queue/queue.cpp
    ${asyncathandler_SOURCE_DIR}/test/mocks/freertos/task/task.cpp
    ${asyncathandler_SOURCE_DIR}/test/mocks/freertos/semaphore/semaphore.cpp
  )
  target_include_directories(FreeRTOS_Sim_Lib PUBLIC
    ${asyncathandler_SOURCE_DIR}/test/mocks
    ${asyncathandler_SOURCE_DIR}/test/mocks/freertos
  )
endif()


# --- ArduinoHttpClient ---
FetchContent_Declare(
  ArduinoHttpClient
  GIT_REPOSITORY https://github.com/arduino-libraries/ArduinoHttpClient
  GIT_TAG master
)
FetchContent_MakeAvailable(ArduinoHttpClient)


# === LIBRARY ===
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  ${LIB_SRC_DIR}/*.cpp
  ${LIB_SRC_DIR}/*.h
)

# Get ArduinoHttpClient source files
file(GLOB_RECURSE ARDUINO_HTTP_CLIENT_SOURCES CONFIGURE_DEPENDS
  ${arduinohttpclient_SOURCE_DIR}/src/*.cpp
  ${arduinohttpclient_SOURCE_DIR}/src/*.h
)

add_library(AsyncGSM STATIC ${SRC_FILES} ${ARDUINO_HTTP_CLIENT_SOURCES})

# Add include directories for AsyncGSM
target_include_directories(AsyncGSM PUBLIC
  ${MOCK_INC_DIR}
  ${LIB_SRC_DIR}
  ${asyncathandler_SOURCE_DIR}/src
  ${arduinohttpclient_SOURCE_DIR}/src
)

target_include_directories(AsyncGSM PRIVATE
  ${asyncathandler_SOURCE_DIR}/test/mocks
  ${asyncathandler_SOURCE_DIR}/test/mocks/freertos
  ${gmock_SOURCE_DIR}/include
  ${gtest_SOURCE_DIR}/include
)

# Link FreeRTOS-Sim to your main library so all files can use it
target_link_libraries(AsyncGSM PUBLIC
  FreeRTOS_Sim_Lib
  AsyncATHandler
)
enable_language(C)

set(LOG_LEVEL "3" CACHE STRING "Log level (0-5, where 5 is most verbose)")
set_property(CACHE LOG_LEVEL PROPERTY STRINGS "0" "1" "2" "3" "4" "5")
# Set the log level for AsyncATHandler
target_compile_definitions(AsyncATHandler PUBLIC LOG_LEVEL=${LOG_LEVEL})

# === TESTS ===
if(ASYNCGSM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

if(ASYNCGSM_ENABLE_EXAMPLES_INDEXING)
  add_subdirectory(examples)
endif()

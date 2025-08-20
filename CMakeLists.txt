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

# Paths
set(LIB_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(MOCK_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test/mocks)
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)

# === NATIVE BUILD MODE ===
message(STATUS "🔧 Building with mock Arduino/FreeRTOS headers")

# === DEPENDENCIES ===
include(FetchContent)
cmake_policy(SET CMP0135 NEW)

# --- FreeRTOS-Sim ---
FetchContent_Declare(
  FreeRTOS_Sim
  GIT_REPOSITORY https://github.com/Nathan-ma/FreeRTOS-Sim
  GIT_TAG macos/v9.0.0
)
FetchContent_MakeAvailable(FreeRTOS_Sim)

# --- AsyncATHandler ---
# Disable AsyncATHandler's tests to avoid building them in AsyncGSM
set(ASYNCAT_HANDLER_BUILD_TESTS OFF CACHE BOOL "Disable AsyncATHandler tests" FORCE)

FetchContent_Declare(
  AsyncATHandler
  GIT_REPOSITORY https://github.com/ByteNana/AsyncATHandler
  GIT_TAG dev
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

# === FREERTOS-SIM LIBRARY ===
# Create FreeRTOS-Sim library from source files
file(GLOB_RECURSE FREERTOS_SIM_SOURCES
  ${freertos_sim_SOURCE_DIR}/Source/*.c
  ${freertos_sim_SOURCE_DIR}/Source/portable/GCC/Posix/*.c
  ${freertos_sim_SOURCE_DIR}/Source/portable/MemMang/heap_4.c
)

# Remove main.c if it exists to avoid conflicts
list(FILTER FREERTOS_SIM_SOURCES EXCLUDE REGEX ".*main\\.c$")
# Exclude heap_1.c as we are using heap_4.c
list(FILTER FREERTOS_SIM_SOURCES EXCLUDE REGEX ".*heap_1\\.c$")

add_library(AsyncGSM_FreeRTOS_Sim_Lib STATIC ${FREERTOS_SIM_SOURCES})

# Set FreeRTOS-Sim include directories
target_include_directories(AsyncGSM_FreeRTOS_Sim_Lib PUBLIC
  ${freertos_sim_SOURCE_DIR}/Source/include
  ${freertos_sim_SOURCE_DIR}/Source/portable/GCC/Posix
  ${TEST_DIR}/mocks/freertos
  ${freertos_sim_SOURCE_DIR}
)

# Add compiler definitions for FreeRTOS-Sim
target_compile_definitions(AsyncGSM_FreeRTOS_Sim_Lib PUBLIC
  -DprojCOVERAGE_TEST=0
  -DRUN_TIME_STATS_GATHERING=0
)

# Link pthread for POSIX port
find_package(Threads REQUIRED)
target_link_libraries(AsyncGSM_FreeRTOS_Sim_Lib PUBLIC Threads::Threads)

# === INCLUDE DIRECTORIES ===
include_directories(
  ${MOCK_INC_DIR}
  ${LIB_SRC_DIR}
  ${freertos_sim_SOURCE_DIR}/Source/include
  ${freertos_sim_SOURCE_DIR}/Source/portable/GCC/Posix
  ${freertos_sim_SOURCE_DIR}/Project
  ${freertos_sim_SOURCE_DIR}
  ${asyncathandler_SOURCE_DIR}/test/mocks  # Arduino mocks from AsyncATHandler
  ${arduinohttpclient_SOURCE_DIR}/src       # ArduinoHttpClient headers
  ${gmock_SOURCE_DIR}/include
  ${gtest_SOURCE_DIR}/include
  ${Unity_SOURCE_DIR}/src
)

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
  ${LIB_SRC_DIR}
  ${asyncathandler_SOURCE_DIR}/src  # AsyncATHandler headers
  ${arduinohttpclient_SOURCE_DIR}/src  # ArduinoHttpClient headers
)

target_include_directories(AsyncGSM PRIVATE
  ${MOCK_INC_DIR}
  ${asyncathandler_SOURCE_DIR}/test/mocks  # Arduino mocks
)

# Link FreeRTOS-Sim to your main library so all files can use it
target_link_libraries(AsyncGSM PUBLIC
  AsyncGSM_FreeRTOS_Sim_Lib
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

  # Get any existing mock FreeRTOS sources (keeping for backward compatibility)
  file(GLOB_RECURSE EXISTING_FREERTOS_SOURCES CONFIGURE_DEPENDS
    ${MOCK_INC_DIR}/freertos/FreeRTOS.cpp
    ${MOCK_INC_DIR}/freertos/**/*.cpp
    ${MOCK_INC_DIR}/freertos/**/*.c
  )

  # Only get test files from the TEST_DIR
  file(GLOB TEST_FILES CONFIGURE_DEPENDS
    ${TEST_DIR}/test_native/*.cpp
  )

  foreach(TEST_FILE ${TEST_FILES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    set(EXEC_NAME "${TEST_NAME}_test_exec")

    set(TEST_SRC ${TEST_FILE})
    # Add existing mock sources if they exist
    if(EXISTING_FREERTOS_SOURCES)
      list(APPEND TEST_SRC ${EXISTING_FREERTOS_SOURCES})
    endif()

    add_executable(${EXEC_NAME} ${TEST_SRC} ${MOCK_INC_DIR}/freertos/freertos_hooks.c)

    target_link_libraries(${EXEC_NAME}
      PRIVATE
      AsyncGSM
      AsyncATHandler
      AsyncGSM_FreeRTOS_Sim_Lib
      gmock
      gtest
      gtest_main
      unity
    )

    add_test(NAME ${EXEC_NAME} COMMAND ${EXEC_NAME})
  endforeach()
endif()

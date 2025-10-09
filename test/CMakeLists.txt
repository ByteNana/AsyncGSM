cmake_minimum_required(VERSION 3.15)

# Build tests using AsyncATHandler's mocks and our local shims

add_compile_definitions(LOG_LEVEL=${LOG_LEVEL} LOGGING=1)
enable_testing()

set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(MOCK_INC_DIR ${TEST_DIR}/mocks)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

# Collect test sources
file(GLOB TEST_FILES CONFIGURE_DEPENDS
  ${TEST_DIR}/test_native/*.cpp
)

file(GLOB TEST_MOCK_SOURCES CONFIGURE_DEPENDS
  ${MOCK_INC_DIR}/*.cpp
  ${MOCK_INC_DIR}/*.c
)

foreach(TEST_FILE ${TEST_FILES})
  get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
  set(EXEC_NAME "${TEST_NAME}_test_exec")

  add_executable(${EXEC_NAME} ${TEST_FILE} ${TEST_MOCK_SOURCES})

  target_include_directories(${EXEC_NAME} PRIVATE
    ${LIB_SRC_DIR}
    ${TEST_DIR}/common
    ${asyncathandler_SOURCE_DIR}/src
    ${asyncathandler_SOURCE_DIR}/test/mocks
    ${asyncathandler_SOURCE_DIR}/test/mocks/freertos
    ${arduinohttpclient_SOURCE_DIR}/src
  )

  target_link_libraries(${EXEC_NAME}
    PRIVATE
    AsyncGSM
    AsyncATHandler
    FreeRTOS_Sim_Lib
    gmock
    gtest
    gtest_main
    unity
  )

  add_test(NAME ${EXEC_NAME} COMMAND ${EXEC_NAME})
  # Ensure hung tests do not stall CI/local runs indefinitely
  set_tests_properties(${EXEC_NAME} PROPERTIES TIMEOUT 30)
endforeach()

# Unit Testing Improvements for AsyncGSM

## Overview
This issue tracks the comprehensive improvements needed for the unit testing infrastructure of the AsyncGSM library. The goal is to ensure robust test coverage across all components, fix existing build issues, and establish best practices for ongoing test development.

## Current State

### Existing Tests
- **Native Tests** (GoogleTest/GoogleMock in `test/test_native/`):
  - `test_async_gsm_gprs.cpp` - Tests GPRS connection functionality (~145 lines)
  - `test_async_gsm_http_client.cpp` - Tests HTTP client integration (~336 lines)
  
- **Hardware Tests** (Unity framework in `examples/`):
  - `examples/gsm/test/test_gsm/` - Hardware integration tests for GSM functionality
  - `examples/http_client/test/test_http_client/` - Hardware integration tests for HTTP client

### Build Issues
- ❌ Native tests currently fail to build due to FreeRTOS-Sim portmacro.h include issues
- ❌ CMake configuration needs fixing for proper FreeRTOS-Sim integration
- The CI workflow expects tests to pass but currently cannot build

## Missing Test Coverage

### Core Components Without Tests

1. **AsyncGSM.cpp** - Basic GSM modem operations
   - [ ] `init()` - Stream initialization
   - [ ] `begin()` - Modem initialization with APN
   - [ ] `getSimCCID()` - SIM card identification
   - [ ] `getIMEI()` - Device IMEI retrieval
   - [ ] `getOperator()` - Network operator information
   - [ ] `getIPAddress()` - IP address retrieval
   - [ ] `connect()` - TCP/UDP connection establishment
   - [ ] `write()` - Data transmission
   - [ ] `read()` - Data reception
   - [ ] `available()` - Check available data
   - [ ] `connected()` - Connection status check
   - [ ] `stop()` - Connection termination

2. **AsyncSecureGSM.cpp** - Secure SSL/TLS connections
   - [ ] SSL connection establishment
   - [ ] Certificate management (`setCertificate()`)
   - [ ] Secure data transmission
   - [ ] Secure connection termination

3. **AsyncMqttGSM.cpp** - MQTT functionality
   - [ ] `init()` - MQTT client initialization
   - [ ] `setServer()` - MQTT broker configuration
   - [ ] `connect()` - MQTT broker connection
   - [ ] `publish()` - Message publishing
   - [ ] `subscribe()` - Topic subscription
   - [ ] `unsubscribe()` - Topic unsubscription
   - [ ] `loop()` - Message processing loop
   - [ ] Callback functionality
   - [ ] Reconnection logic

4. **EG915 Modem Driver** (`src/EG915/`)
   - [ ] `EG915.cpp` - Core modem operations
   - [ ] `EG915.modem.cpp` - Modem-specific commands
   - [ ] `EG915.secure.cpp` - Security operations
   - [ ] `EG915.urc.cpp` - Unsolicited Result Code handling

5. **MqttQueue** (`src/MqttQueue/`)
   - [ ] Queue operations (push, pop, peek)
   - [ ] Thread safety with atomics
   - [ ] Memory management

## Required Improvements

### 1. Fix Build System (HIGH PRIORITY)
- [ ] Fix FreeRTOS-Sim CMake configuration
- [ ] Ensure `portmacro.h` is properly included
- [ ] Verify build works on Ubuntu (CI environment)
- [ ] Update CMakeLists.txt if needed for proper include paths

### 2. Expand Native Test Coverage
- [ ] Add unit tests for all AsyncGSM public methods
- [ ] Add unit tests for AsyncSecureGSM
- [ ] Add comprehensive unit tests for AsyncMqttGSM
- [ ] Add tests for EG915 modem driver components
- [ ] Add tests for MqttQueue data structure

### 3. Improve Test Infrastructure
- [ ] Create helper utilities for common test scenarios
- [ ] Improve MockStream to better simulate real GSM responses
- [ ] Add test fixtures for common setup/teardown
- [ ] Document testing best practices in CONTRIBUTING.md

### 4. Hardware Test Improvements
- [ ] Ensure hardware tests cover all examples
- [ ] Add more comprehensive assertions
- [ ] Document hardware test requirements

### 5. CI/CD Integration
- [ ] Ensure `make test` passes in CI
- [ ] Add code coverage reporting (optional but recommended)
- [ ] Ensure formatting checks pass before tests run

## Test Quality Guidelines

### Test Characteristics
- **Isolated**: Each test should be independent
- **Repeatable**: Tests must produce consistent results
- **Fast**: Native tests should execute quickly
- **Comprehensive**: Cover normal cases, edge cases, and error conditions
- **Maintainable**: Clear naming and documentation

### Mock Strategy
- Use GoogleMock `MockStream` for simulating serial communication
- Mock AT command responses realistically
- Test both success and failure scenarios
- Simulate timing and asynchronous behavior

### Naming Conventions
- Test files: `test_<component>.cpp`
- Test cases: `TEST_F(ComponentTest, MethodName_Scenario_ExpectedBehavior)`
- Example: `TEST_F(AsyncGSMTest, GetSimCCID_ValidResponse_ReturnsCorrectCCID)`

## Success Criteria

- [ ] All tests build successfully with `make build`
- [ ] All tests pass with `make test`
- [ ] CI pipeline succeeds on all PRs
- [ ] Test coverage includes at least 80% of public API methods
- [ ] All new features include corresponding tests
- [ ] Documentation updated to reflect testing practices

## Implementation Plan

### Phase 1: Fix Build Issues (Week 1)
1. Diagnose and fix FreeRTOS-Sim include problems
2. Ensure existing tests build and pass
3. Update CI to verify test success

### Phase 2: Core Coverage (Weeks 2-3)
1. Add comprehensive tests for AsyncGSM
2. Add tests for AsyncSecureGSM
3. Ensure all tests pass reliably

### Phase 3: Extended Coverage (Week 4)
1. Add comprehensive tests for AsyncMqttGSM
2. Add tests for EG915 components
3. Add tests for MqttQueue

### Phase 4: Polish and Documentation (Week 5)
1. Refactor and improve test utilities
2. Update documentation
3. Add code coverage reporting (optional)

## Resources

### Testing Frameworks
- **GoogleTest/GoogleMock**: Native C++ unit testing
- **Unity**: Embedded hardware testing
- **FreeRTOS-Sim**: RTOS simulation for native tests

### Documentation
- [GoogleTest Primer](https://google.github.io/googletest/primer.html)
- [GoogleMock for Dummies](https://google.github.io/googletest/gmock_for_dummies.html)
- [Unity Test Framework](https://github.com/ThrowTheSwitch/Unity)

## Related Issues
- Build system needs FreeRTOS-Sim configuration fix
- MQTT functionality needs comprehensive testing
- Secure GSM (SSL/TLS) needs validation

## Notes
- This is a comprehensive tracking issue - it can be broken down into smaller issues as needed
- Priority should be given to fixing the build system first
- Tests should align with the existing `make test` command used in CI

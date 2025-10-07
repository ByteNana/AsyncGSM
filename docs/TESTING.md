# Testing Documentation

This directory contains testing documentation and guidelines for the AsyncGSM project.

## Quick Start

### Running Tests

```bash
# Build and run all native tests
make test

# Run only the build
make build

# Test on hardware (requires ESP32 connected)
make esp32-test
```

### Test Structure

```
AsyncGSM/
├── test/
│   ├── test_native/          # Native unit tests (GoogleTest/GoogleMock)
│   │   ├── test_async_gsm_gprs.cpp
│   │   ├── test_async_gsm_http_client.cpp
│   │   └── common.h          # Test utilities and helpers
│   └── mocks/                # Mock implementations for testing
│       ├── Stream.h
│       ├── Client.h
│       └── freertos/         # FreeRTOS simulation mocks
└── examples/
    ├── gsm/test/             # Hardware integration tests
    └── http_client/test/     # HTTP client hardware tests
```

## Writing Tests

### Native Unit Tests

Native tests use GoogleTest and GoogleMock and run on your development machine.

**Example:**
```cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "AsyncGSM.h"
#include "common.h"

class AsyncGSMTest : public FreeRTOSTest {
protected:
  AsyncGSM *gsm;
  NiceMock<MockStream> *mockStream;

  void SetUp() override {
    FreeRTOSTest::SetUp();
    gsm = new AsyncGSM();
    mockStream = new NiceMock<MockStream>();
    mockStream->SetupDefaults();
  }

  void TearDown() override {
    delete gsm;
    delete mockStream;
    FreeRTOSTest::TearDown();
  }
};

TEST_F(AsyncGSMTest, GetSimCCID_ValidResponse_ReturnsCorrectCCID) {
  // Arrange
  mockStream->InjectRxData("AT+QCCID\r\n+QCCID: 89012345678901234567\r\nOK\r\n");
  gsm->init(*mockStream);
  
  // Act
  String ccid = gsm->getSimCCID();
  
  // Assert
  EXPECT_EQ("89012345678901234567", ccid);
}
```

### Hardware Tests

Hardware tests use Unity framework and run on actual ESP32 hardware.

**Example:**
```cpp
#include <Arduino.h>
#include <unity.h>
#include "AsyncGSM.h"

AsyncGSM modem;

void test_modem_initialization() {
  bool result = modem.init(Serial1);
  TEST_ASSERT_TRUE(result);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  UNITY_BEGIN();
  RUN_TEST(test_modem_initialization);
  UNITY_END();
}

void loop() {}
```

## Test Guidelines

### Test Naming

- **Test files**: `test_<component>.cpp`
- **Test cases**: `TEST_F(ComponentTest, MethodName_Scenario_ExpectedBehavior)`
- Be descriptive: readers should understand what's being tested

### Test Structure (Arrange-Act-Assert)

```cpp
TEST_F(AsyncGSMTest, Connect_ValidHost_ReturnsSuccess) {
  // Arrange - Set up test conditions
  mockStream->InjectRxData("...");
  gsm->init(*mockStream);
  
  // Act - Execute the functionality
  int result = gsm->connect("example.com", 80);
  
  // Assert - Verify the results
  EXPECT_EQ(1, result);
}
```

### What to Test

1. **Happy path**: Normal operation with valid inputs
2. **Edge cases**: Boundary conditions, empty inputs, maximum values
3. **Error conditions**: Invalid inputs, failure scenarios, timeouts
4. **State transitions**: Verify correct state changes

### Mock Strategy

- Use `MockStream` to simulate serial communication
- Inject realistic AT command responses using `InjectRxData()`
- Test both successful and failed AT responses
- Simulate timing with FreeRTOS delays when needed

### Test Quality

- **Isolated**: Each test should be independent
- **Repeatable**: Same results every time
- **Fast**: Native tests should run in milliseconds
- **Readable**: Clear naming and structure
- **Maintainable**: Easy to update when code changes

## Debugging Tests

### Running Individual Tests

```bash
# Build first
make build

# Run specific test executable
./build/test_async_gsm_gprs_test_exec

# With verbose output
GTEST_COLOR=1 ./build/test_async_gsm_gprs_test_exec --gtest_filter="AsyncGSMTest.GprsConnect*"
```

### Common Issues

1. **FreeRTOS timing issues**: Add appropriate delays with `vTaskDelay()`
2. **Mock not responding**: Ensure `InjectRxData()` is called before the operation
3. **Memory leaks**: Check SetUp/TearDown properly clean up resources
4. **Flaky tests**: Usually timing-related, increase delays or use better synchronization

## Coverage Goals

- **Minimum**: 70% code coverage
- **Target**: 80%+ code coverage
- **Focus**: Public API methods should have comprehensive coverage

## CI Integration

Tests run automatically on every push and pull request:

1. Code formatting check (`make check-format`)
2. Native test build (`cmake --build build`)
3. Native test execution (`ctest`)
4. PlatformIO build verification (`make esp32`)

## Contributing Tests

When adding new functionality:

1. Write tests for the new feature
2. Ensure all tests pass locally with `make test`
3. Run formatting with `make format`
4. Include test information in your PR description

## References

- [GoogleTest Documentation](https://google.github.io/googletest/)
- [GoogleMock for Dummies](https://google.github.io/googletest/gmock_for_dummies.html)
- [Unity Framework Guide](https://github.com/ThrowTheSwitch/Unity)
- [FreeRTOS-Sim](https://github.com/Nathan-ma/FreeRTOS-Sim)

## Need Help?

- Check existing tests in `test/test_native/` for examples
- Read the main issue tracking testing improvements: `UNIT_TESTING_ISSUE.md`
- Open an issue using the unit testing template in `.github/ISSUE_TEMPLATE/`

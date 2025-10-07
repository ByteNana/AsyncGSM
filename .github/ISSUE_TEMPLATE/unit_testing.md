---
name: Unit Testing Enhancement
about: Track unit testing improvements and coverage for AsyncGSM components
title: '[TEST] Unit Testing Improvements'
labels: testing, enhancement
assignees: ''

---

## Component Being Tested
<!-- Which component needs testing? e.g., AsyncGSM, AsyncMqttGSM, EG915, etc. -->

## Current State
<!-- What is the current test coverage for this component? -->
- [ ] No tests exist
- [ ] Some tests exist but incomplete
- [ ] Tests exist but failing

## Test Requirements

### Methods/Functions to Test
<!-- List specific methods or functions that need test coverage -->
- [ ] Method 1
- [ ] Method 2
- [ ] Method 3

### Test Scenarios
<!-- What scenarios should be tested? -->

#### Happy Path
<!-- Normal operation scenarios -->
- [ ] Scenario 1
- [ ] Scenario 2

#### Edge Cases
<!-- Boundary conditions and unusual inputs -->
- [ ] Edge case 1
- [ ] Edge case 2

#### Error Handling
<!-- Failure scenarios and error conditions -->
- [ ] Error scenario 1
- [ ] Error scenario 2

## Implementation Details

### Test Type
- [ ] Native unit test (GoogleTest/GoogleMock in `test/test_native/`)
- [ ] Hardware integration test (Unity in `examples/*/test/`)

### Mock Requirements
<!-- What needs to be mocked? -->
- [ ] Serial/Stream communication
- [ ] AT command responses
- [ ] Network responses
- [ ] Other: _____

### Dependencies
<!-- Are there other components or issues this depends on? -->
- Related to issue #___
- Depends on #___

## Acceptance Criteria
- [ ] Tests compile without errors
- [ ] Tests pass consistently
- [ ] Tests cover success scenarios
- [ ] Tests cover failure scenarios
- [ ] Tests are documented with clear names
- [ ] CI pipeline passes with new tests

## Additional Context
<!-- Add any other context, screenshots, or examples -->

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "AsyncGSM.h"
#include "Stream.h"
#include "common.h"

using ::testing::NiceMock;

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
    if (gsm) {
      delete gsm;
      gsm = nullptr;
    }
    if (mockStream) {
      delete mockStream;
      mockStream = nullptr;
    }
    FreeRTOSTest::TearDown();
  }
};

TEST_F(AsyncGSMTest, GprsConnectSendsCommands) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        // Initialize GSM
        if (!gsm->init(*mockStream)) {
          throw std::runtime_error("GSM init failed");
        }

        // Pre-inject all expected responses in sequence
        // 1. AT+QIDEACT=1 (disconnect first)
        mockStream->InjectRxData("AT+QIDEACT=1\r\n");
        mockStream->InjectRxData("OK\r\n");

        // 2. AT+QICSGP=1,1,"apn","user","pwd" (configure PDP context)
        mockStream->InjectRxData("AT+QICSGP=1,1,\"apn\",\"user\",\"pwd\"\r\n");
        mockStream->InjectRxData("OK\r\n");

        // 3. AT+QIACT=1 (activate PDP context)
        mockStream->InjectRxData("AT+QIACT=1\r\n");
        mockStream->InjectRxData("OK\r\n");

        // 4. AT+QIACT? (query PDP context - THIS WAS MISSING!)
        mockStream->InjectRxData("AT+QIACT?\r\n");
        mockStream->InjectRxData("+QIACT: 1,1,1,\"192.168.1.100\"\r\n");
        mockStream->InjectRxData("OK\r\n");

        // 5. AT+CGATT=1 (attach to GPRS)
        mockStream->InjectRxData("AT+CGATT=1\r\n");
        mockStream->InjectRxData("OK\r\n");

        // Execute the GPRS connect
        bool result = gsm->gprsConnect("apn", "user", "pwd");

        if (!result) {
          throw std::runtime_error("GPRS connect should have succeeded");
        }

        // Verify the commands were sent in the right order
        std::string sentData = mockStream->GetTxData();

        if (sentData.find("AT+QIDEACT=1") == std::string::npos) {
          throw std::runtime_error("AT+QIDEACT=1 command not sent");
        }

        if (sentData.find("AT+QICSGP=1,1,\"apn\",\"user\",\"pwd\"") ==
            std::string::npos) {
          throw std::runtime_error("AT+QICSGP command not sent correctly");
        }

        if (sentData.find("AT+QIACT=1") == std::string::npos) {
          throw std::runtime_error("AT+QIACT=1 command not sent");
        }

        if (sentData.find("AT+QIACT?") == std::string::npos) {
          throw std::runtime_error("AT+QIACT? command not sent");
        }

        if (sentData.find("AT+CGATT=1") == std::string::npos) {
          throw std::runtime_error("AT+CGATT=1 command not sent");
        }

        log_i("[Test] GPRS connect test passed - all commands sent correctly");
      },
      "GprsConnectTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncGSMTest, GprsConnectWithNullCredentials) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!gsm->init(*mockStream)) {
          throw std::runtime_error("GSM init failed");
        }

        // Test with null user/password (should use empty strings)
        mockStream->InjectRxData("AT+QIDEACT=1\r\n");
        mockStream->InjectRxData("OK\r\n");

        mockStream->InjectRxData("AT+QICSGP=1,1,\"internet\",\"\",\"\"\r\n");
        mockStream->InjectRxData("OK\r\n");

        mockStream->InjectRxData("AT+QIACT=1\r\n");
        mockStream->InjectRxData("OK\r\n");

        // Add the missing QIACT? response
        mockStream->InjectRxData("AT+QIACT?\r\n");
        mockStream->InjectRxData("+QIACT: 1,1,1,\"10.0.0.1\"\r\n");
        mockStream->InjectRxData("OK\r\n");

        mockStream->InjectRxData("AT+CGATT=1\r\n");
        mockStream->InjectRxData("OK\r\n");

        bool result = gsm->gprsConnect("internet", nullptr, nullptr);

        if (!result) {
          throw std::runtime_error(
              "GPRS connect with null credentials should succeed");
        }

        std::string sentData = mockStream->GetTxData();

        // Should use empty strings for null user/password
        if (sentData.find("AT+QICSGP=1,1,\"internet\",\"\",\"\"") ==
            std::string::npos) {
          throw std::runtime_error("Should handle null credentials correctly");
        }
      },
      "GprsConnectNullCredsTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncGSMTest, GprsConnectFailureHandling) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!gsm->init(*mockStream)) {
          throw std::runtime_error("GSM init failed");
        }

        // Simulate failure at QICSGP step
        mockStream->InjectRxData("AT+QIDEACT=1\r\n");
        mockStream->InjectRxData("OK\r\n");

        mockStream->InjectRxData("AT+QICSGP=1,1,\"badapn\",\"\",\"\"\r\n");
        mockStream->InjectRxData("ERROR\r\n"); // Simulate failure

        bool result = gsm->gprsConnect("badapn");
        EXPECT_FALSE(result);

        std::string sentData = mockStream->GetTxData();
        if (sentData.find("AT+QICSGP=1,1,\"badapn\",\"\",\"\"") ==
            std::string::npos) {
          throw std::runtime_error("Should handle null credentials correctly");
        }

        log_i("[Test] GPRS connect failure handling works correctly");
      },
      "GprsConnectFailureTest");

  EXPECT_TRUE(testResult);
}

FREERTOS_TEST_MAIN()

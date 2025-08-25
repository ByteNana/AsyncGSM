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

  void runModemSetupAndGprsConnect(bool expectSuccess) {
    // Create a separate task to handle injecting responses
    std::atomic<bool> responderDone{false};
    TaskHandle_t responderTaskHandle = nullptr;

    auto responderTask = [](void *params) {
      auto *data = static_cast<
          std::tuple<NiceMock<MockStream> *, std::atomic<bool> *> *>(params);
      auto *mockStream = std::get<0>(*data);
      auto *responderDone = std::get<1>(*data);

      // Wait for gsm->begin() to run and send all its commands
      vTaskDelay(pdMS_TO_TICKS(100));
      mockStream->InjectRxData("AT\r\nOK\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));
      mockStream->InjectRxData("ATE0\r\nOK\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));
      mockStream->InjectRxData("AT+CMEE=2\r\nOK\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));
      mockStream->InjectRxData("AT+CGMM\r\nEG915U\r\nOK\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));
      mockStream->InjectRxData("AT+CGMI\r\nQuectel\r\nOK\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));
      mockStream->InjectRxData("AT+CTZU=1\r\nOK\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));
      mockStream->InjectRxData("AT+CPIN?\r\n+CPIN: READY\r\nOK\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));
      for (int i = 0; i < 4; i++) {
        mockStream->InjectRxData(String("AT+QICLOSE=") + String(i) +
                                 "\r\nOK\r\n");
        vTaskDelay(pdMS_TO_TICKS(100));
      }
      mockStream->InjectRxData("AT+QSCLK=0\r\nOK\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));

      // Wait for gprsConnect() to run and send its commands
      mockStream->InjectRxData("AT+QIDEACT=1\r\nOK\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));
      mockStream->InjectRxData(
          "AT+QICSGP=1,1,\"apn\",\"user\",\"pwd\"\r\nOK\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));
      mockStream->InjectRxData("AT+QIACT=1\r\nOK\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));
      mockStream->InjectRxData(
          "AT+QIACT?\r\n+QIACT: 1,1,1,\"192.168.1.100\"\r\nOK\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));
      mockStream->InjectRxData("AT+CGATT=1\r\nOK\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));
      mockStream->InjectRxData("+CGATT:1\r\nOK\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));
      mockStream->InjectRxData("AT+QIDEACT=1\r\nOK\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));
      mockStream->InjectRxData("AT+QICSGP\r\nOK\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));
      mockStream->InjectRxData("AT+QIACT=1\r\nOK\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));
      mockStream->InjectRxData("+QIACT:1,1\r\nOK\r\n");
      vTaskDelay(pdMS_TO_TICKS(100));
      mockStream->InjectRxData("+CGATT:1\r\nOK\r\n");

      *responderDone = true;
      vTaskDelete(nullptr);
    };

    auto params = std::make_tuple(mockStream, &responderDone);
    xTaskCreate(responderTask, "ResponderTask", configMINIMAL_STACK_SIZE * 4,
                &params, 1, &responderTaskHandle);

    bool gsmInitSuccess = gsm->init(*mockStream);
    if (!gsmInitSuccess) {
      throw std::runtime_error("GSM init failed");
    }

    bool gsmBeginSuccess = gsm->begin("apn");
    if (!gsmBeginSuccess) {
      throw std::runtime_error("GSM begin failed");
    }

    bool gsmConnectSuccess = gsm->modem.gprsConnect("apn", "user", "pwd");

    // Ensure the responder task has finished its work
    while (!responderDone.load()) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (expectSuccess) {
      if (!gsmConnectSuccess) {
        throw std::runtime_error("GPRS connect should have succeeded");
      }
    } else {
      if (gsmConnectSuccess) {
        throw std::runtime_error("GPRS connect should have failed");
      }
    }
  }
};

TEST_F(AsyncGSMTest, GprsConnectSendsCommands) {
  bool testResult = runInFreeRTOSTask(
      [this]() { runModemSetupAndGprsConnect(true); }, "GprsConnectTest",
      configMINIMAL_STACK_SIZE * 8, 2, 15000);
  EXPECT_TRUE(testResult);
}

TEST_F(AsyncGSMTest, GprsConnectWithNullCredentials) {
  bool testResult = runInFreeRTOSTask(
      [this]() { runModemSetupAndGprsConnect(true); },
      "GprsConnectNullCredsTest", configMINIMAL_STACK_SIZE * 8, 2, 15000);
  EXPECT_TRUE(testResult);
}

FREERTOS_TEST_MAIN()

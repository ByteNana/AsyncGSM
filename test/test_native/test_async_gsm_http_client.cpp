#include "AsyncGSM.h"
#include "Stream.h"
#include "common.h"
#include <HttpClient.h> // ArduinoHttpClient
#include <atomic>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <tuple>

using ::testing::NiceMock;

class AsyncGSMHttpClientTest : public FreeRTOSTest {
protected:
  AsyncGSM *gsm;
  HttpClient *httpClient;
  NiceMock<MockStream> *mockStream;

  void SetUp() override {
    FreeRTOSTest::SetUp();
    gsm = new AsyncGSM();
    mockStream = new NiceMock<MockStream>();
    mockStream->SetupDefaults();
    httpClient = new HttpClient(*gsm, "httpbin.org", 80);
  }

  void TearDown() override {
    try {
      if (httpClient) {
        delete httpClient;
        httpClient = nullptr;
      }
      if (gsm) {
        delete gsm;
        gsm = nullptr;
      }
      if (mockStream) {
        delete mockStream;
        mockStream = nullptr;
      }
    } catch (const std::exception &e) {
      log_e("Exception during TearDown");
      log_e(e.what());
    }
  }

  // A helper function to create and run the responder task.
  void runResponderTask(std::atomic<bool> *done, bool failConnection = false, bool isPost = false, int postPayloadSize = 0) {
    struct ResponderData {
      NiceMock<MockStream> *stream;
      std::atomic<bool> *done;
      bool fail;
      bool isPost;
      int payloadSize;
    };

    auto responderFunction = [](void *pvParameters) {
      auto *data = static_cast<ResponderData *>(pvParameters);

      vTaskDelay(pdMS_TO_TICKS(100));

      while (!data->done->load()) {
        std::string sentData = data->stream->GetTxData();
        data->stream->ClearTxData();

        if (sentData.find("AT+QICLOSE") != std::string::npos) {
          data->stream->InjectRxData("OK\r\n");
          vTaskDelay(pdMS_TO_TICKS(50));
        }

        if (sentData.find("AT+QIDEACT") != std::string::npos) {
          data->stream->InjectRxData("OK\r\n");
          vTaskDelay(pdMS_TO_TICKS(50));
        }

        if (sentData.find("AT+QIOPEN") != std::string::npos) {
          data->stream->InjectRxData("OK\r\n");
          if (data->fail) {
            data->stream->InjectRxData("+QIOPEN: 0,1\r\n");
          } else {
            data->stream->InjectRxData("+QIOPEN: 0,0\r\n");
          }
          vTaskDelay(pdMS_TO_TICKS(50));
        }

        if (sentData.find("AT+QISEND") != std::string::npos) {
          data->stream->InjectRxData(">\r\n");
          vTaskDelay(pdMS_TO_TICKS(100));
          data->stream->InjectRxData("OK\r\n");
          data->stream->InjectRxData("SEND OK\r\n");
          vTaskDelay(pdMS_TO_TICKS(50));
        }

        if (sentData.find("AT+QIRD") != std::string::npos) {
          if (data->isPost) {
            data->stream->InjectRxData(
                "+QIURC: \"recv\",0,109,\"220.180.239.212\",8062,\""
                "HTTP/1.1 201 Created\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: 30\r\n"
                "\r\n"
                "{\"status\":\"created\",\"id\":123}\r\n"
                "\r\n\"");
          } else {
            data->stream->InjectRxData(
                "+QIURC: \"recv\",0,120,\"220.180.239.212\",8062,\""
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: 45\r\n"
                "\r\n"
                "{\"args\":{},\"headers\":{},\"url\":\"httpbin.org\"}\r\n"
                "\r\n\"");
          }
          vTaskDelay(pdMS_TO_TICKS(50));
        }

        vTaskDelay(pdMS_TO_TICKS(10));
      }

      delete data;
      vTaskDelete(nullptr);
    };

    auto *data = new ResponderData{mockStream, done, failConnection, isPost, postPayloadSize};
    xTaskCreate(responderFunction, "ResponderTask", configMINIMAL_STACK_SIZE * 4, data, 1, nullptr);
  }
};

TEST_F(AsyncGSMHttpClientTest, HttpGetRequest) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        std::atomic<bool> responderDone{false};
        runResponderTask(&responderDone);

        try {
          if (!gsm->init(*mockStream)) {
            throw std::runtime_error("GSM init failed");
          }

          log_i("[Test] Starting HTTP GET request...");
          int connectionResult = httpClient->get("/get");

          if (connectionResult != 0) {
            throw std::runtime_error("HTTP connection failed");
          }

          int statusCode = httpClient->responseStatusCode();
          if (statusCode != 200) {
            throw std::runtime_error("Expected HTTP 200, got: " + String(statusCode));
          }

          String responseBody = httpClient->responseBody();
          if (responseBody.indexOf("httpbin.org") == -1) {
            throw std::runtime_error("Response validation failed");
          }
        } catch (const std::exception &e) {
          responderDone = true;
          vTaskDelay(pdMS_TO_TICKS(500));
          throw;
        }

        responderDone = true;
        vTaskDelay(pdMS_TO_TICKS(500));
      },
      "HttpGetTest", 8192, 2, 15000);

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncGSMHttpClientTest, HttpPostWithJson) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        String jsonPayload = "{\"temperature\":23.5,\"humidity\":65.2}";
        std::atomic<bool> responderDone{false};
        runResponderTask(&responderDone, false, true, jsonPayload.length());

        try {
          if (!gsm->init(*mockStream)) {
            throw std::runtime_error("GSM init failed");
          }

          log_i("[Test] Starting HTTP POST request...");

          httpClient->beginRequest();
          httpClient->post("/post");
          httpClient->sendHeader("Content-Type", "application/json");
          httpClient->sendHeader("Content-Length", jsonPayload.length());
          httpClient->beginBody();
          httpClient->print(jsonPayload);
          httpClient->endRequest();

          vTaskDelay(pdMS_TO_TICKS(500));

          int statusCode = httpClient->responseStatusCode();
          if (statusCode != 201) {
            throw std::runtime_error("Expected HTTP 201, got: " + String(statusCode));
          }

          String responseBody = httpClient->responseBody();
          if (responseBody.indexOf("created") == -1) {
            throw std::runtime_error("POST response validation failed");
          }
        } catch (const std::exception &e) {
          responderDone = true;
          vTaskDelay(pdMS_TO_TICKS(500));
          throw;
        }

        responderDone = true;
        vTaskDelay(pdMS_TO_TICKS(500));
      },
      "HttpPostTest", 8192, 2, 15000);

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncGSMHttpClientTest, HttpConnectionFailure) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        std::atomic<bool> responderDone{false};
        runResponderTask(&responderDone, true);

        try {
          if (!gsm->init(*mockStream)) {
            throw std::runtime_error("GSM init failed");
          }

          log_i("[Test] Testing connection failure...");

          int connectionResult = httpClient->get("/test");

          if (connectionResult == 0) {
            printf("Connection Result: %d\n", connectionResult);
            throw std::runtime_error("Expected connection failure");
          }
        } catch (const std::exception &e) {
          responderDone = true;
          vTaskDelay(pdMS_TO_TICKS(500));
          throw;
        }

        responderDone = true;
        vTaskDelay(pdMS_TO_TICKS(500));
      },
      "ConnectionFailureTest", 8192, 2, 10000);

  EXPECT_TRUE(testResult);
}

FREERTOS_TEST_MAIN()

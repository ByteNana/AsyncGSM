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
  void runResponderTask(std::atomic<bool> *done, bool failConnection = false,
                        bool isPost = false, int postPayloadSize = 0) {
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

        vTaskDelay(pdMS_TO_TICKS(10));
      }

      delete data;
      vTaskDelete(nullptr);
    };

    auto *data = new ResponderData{mockStream, done, failConnection, isPost,
                                   postPayloadSize};
    xTaskCreate(responderFunction, "ResponderTask",
                configMINIMAL_STACK_SIZE * 4, data, 1, nullptr);
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
          scheduleInject(mockStream, 2600,
                         "+QIRD: 1474\r\n"
                         "HTTP/1.1 200 OK\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: 1858\r\n"
                         "\r\n"
                         "{ \"items\": ["
                         "{\"id\":1,\"val\":\"foo\"},"
                         "{\"id\":2,\"val\":\"bar\"},"
                         "{\"id\":3,\"val\":\"baz\"},"
                         "{\"id\":4,\"val\":\"qux\"},"
                         "{\"id\":5,\"val\":\"quux\"},"
                         "{\"id\":6,\"val\":\"corge\"},"
                         "{\"id\":7,\"val\":\"grault\"},"
                         "{\"id\":8,\"val\":\"garply\"},"
                         "{\"id\":9,\"val\":\"waldo\"},"
                         "{\"id\":10,\"val\":\"fred\"},"
                         "{\"id\":11,\"val\":\"plugh\"},"
                         "{\"id\":12,\"val\":\"xyzzy\"},"
                         "{\"id\":13,\"val\":\"thud\"},"
                         "{\"id\":14,\"val\":\"alpha\"},"
                         "{\"id\":15,\"val\":\"beta\"},"
                         "{\"id\":16,\"val\":\"gamma\"},"
                         "{\"id\":17,\"val\":\"delta\"},"
                         "{\"id\":18,\"val\":\"epsilon\"},"
                         "{\"id\":19,\"val\":\"zeta\"},"
                         "{\"id\":20,\"val\":\"eta\"},"
                         "{\"id\":21,\"val\":\"theta\"},"
                         "{\"id\":22,\"val\":\"iota\"},"
                         "{\"id\":23,\"val\":\"kappa\"},"
                         "{\"id\":24,\"val\":\"lambda\"},"
                         "{\"id\":25,\"val\":\"mu\"},"
                         "{\"id\":26,\"val\":\"nu\"},"
                         "{\"id\":27,\"val\":\"xi\"},"
                         "{\"id\":28,\"val\":\"omicron\"},"
                         "{\"id\":29,\"val\":\"pi\"},"
                         "{\"id\":30,\"val\":\"rho\"},"
                         "{\"id\":31,\"val\":\"sigma\"},"
                         "{\"id\":32,\"val\":\"tau\"},"
                         "{\"id\":33,\"val\":\"upsilon\"},"
                         "{\"id\":34,\"val\":\"phi\"},"
                         "{\"id\":35,\"val\":\"chi\"},"
                         "{\"id\":36,\"val\":\"psi\"},"
                         "{\"id\":37,\"val\":\"omega\"},"
                         "{\"id\":38,\"val\":\"foo\"},"
                         "{\"id\":39,\"val\":\"bar\"},"
                         "{\"id\":40,\"val\":\"baz\"},"
                         "{\"id\":41,\"val\":\"qux\"},"
                         "{\"id\":42,\"val\":\"quux\"},"
                         "{\"id\":43,\"val\":\"corge\"},"
                         "{\"id\":44,\"val\":\"grault\"},"
                         "{\"id\":45,\"val\":\"garply\"},"
                         "{\"id\":46,\"val\":\"waldo\"},"
                         "{\"id\":47,\"val\":\"fred\"},"
                         "{\"id\":48,\"val\":\"plugh\"},"
                         "{\"id\":49,\"val\":\"xyzzy\"},"
                         "{\"id\":50,\"val\":\"thud\"},"
                         "{\"id\":51,\"val\":\"alpha\"},"
                         "{\"id\":52,\"val\":\"beta\"},"
                         "{\"id\":53,\"val\":\"gamma\"},"
                         "{\"id\":54,\"val\":\"delta\"},"
                         "{\"id\":55,\"val\":\"epsilon\"},"
                         "{\"id\":56,\"val\":\"zeta\"},"
                         "{\"id\":57,\"val\":\"eta\"},"
                         "{\"id\":58,\"val\":\"theta\"},"
                         "{\"id\":59,\"val\":\"iota\"},"
                         "{\"id\":60,\"val\":\"kappa\"},\r\nOK\r\n");
          scheduleInject(mockStream, 2700,
                         "+QIRD: 455\r\n"
                         "{\"id\":61,\"val\":\"lambda\"},"
                         "{\"id\":62,\"val\":\"mu\"},"
                         "{\"id\":63,\"val\":\"nu\"},"
                         "{\"id\":64,\"val\":\"xi\"},"
                         "{\"id\":65,\"val\":\"omicron\"},"
                         "{\"id\":66,\"val\":\"pi\"},"
                         "{\"id\":67,\"val\":\"rho\"},"
                         "{\"id\":68,\"val\":\"sigma\"},"
                         "{\"id\":69,\"val\":\"tau\"},"
                         "{\"id\":70,\"val\":\"upsilon\"},"
                         "{\"id\":71,\"val\":\"phi\"},"
                         "{\"id\":72,\"val\":\"chi\"},"
                         "{\"id\":73,\"val\":\"psi\"},"
                         "{\"id\":74,\"val\":\"omega\"},"
                         "{\"id\":75,\"val\":\"foo\"},"
                         "{\"id\":76,\"val\":\"bar\"},"
                         "{\"id\":77,\"val\":\"baz\"},"
                         "{\"id\":78,\"val\":\"qux\"},"
                         "{\"id\":79,\"val\":\"quux\"},"
                         "{\"id\":80,\"val\":\"corge\"}"
                         "]}"
                         "\r\n\r\nOK\r\n");
          scheduleInject(mockStream, 2800, "+QICLOSE: 0\r\n");
          int connectionResult = httpClient->get("/get");

          log_i("Connection result: %d", connectionResult);
          if (connectionResult != 0) {
            throw std::runtime_error("HTTP connection failed");
          }

          int statusCode = httpClient->responseStatusCode();
          if (statusCode != 200) {
            throw std::runtime_error("Expected HTTP 200, got: " +
                                     String(statusCode));
          }

          String responseBody = httpClient->responseBody();
          printf("Response Body: %s\n", responseBody.c_str());
          if (responseBody.indexOf("kappa") == -1) {
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

          scheduleInject(mockStream, 1200,
                         "+QIRD: 109\r\n"
                         "HTTP/1.1 201 Created\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: 30\r\n"
                         "\r\n"
                         "{\"status\":\"created\",\"id\":123}\r\n"
                         "\r\n\"\r\nOK\r\n");
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
            throw std::runtime_error("Expected HTTP 201, got: " +
                                     String(statusCode));
          }

          String responseBody = httpClient->responseBody();
          printf("Response Body: %s\n", responseBody.c_str());
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
          log_i("Connection result: %d", connectionResult);

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

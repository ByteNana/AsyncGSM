#include "AsyncGSM.h"
#include "Stream.h"
#include "common.h"
#include <HttpClient.h> // ArduinoHttpClient
#include <gmock/gmock.h>
#include <gtest/gtest.h>

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
    FreeRTOSTest::TearDown();
  }

  void clearMockStream() {
    mockStream->ClearTxData();
    while (mockStream->available() > 0) {
      mockStream->read();
    }
  }

  void setupConnectionResponses() {
    mockStream->InjectRxData("OK\r\n");
    mockStream->InjectRxData("OK\r\n");
    mockStream->InjectRxData("OK\r\n");
    mockStream->InjectRxData("+QIOPEN: 0,0\r\n");
  }

  void setupQisendResponses(int count) {
    for (int i = 0; i < count; i++) {
      mockStream->InjectRxData(">\r\n");
      mockStream->InjectRxData("SEND OK\r\n");
    }
  }

  void setupHttpResponse(bool isPost = false) {
    // First few QIRD calls return no data
    mockStream->InjectRxData("+QIRD: 0\r\n\r\nOK\r\n");
    mockStream->InjectRxData("+QIRD: 0\r\n\r\nOK\r\n");
    mockStream->InjectRxData("+QIRD: 0\r\n\r\nOK\r\n");

    // Then provide the HTTP response
    String httpResponse;
    if (isPost) {
      httpResponse = "HTTP/1.1 201 Created\r\n"
                     "Content-Type: application/json\r\n"
                     "Content-Length: 30\r\n"
                     "\r\n"
                     "{\"status\":\"created\",\"id\":123}";
    } else {
      httpResponse = "HTTP/1.1 200 OK\r\n"
                     "Content-Type: application/json\r\n"
                     "Content-Length: 45\r\n"
                     "\r\n"
                     "{\"args\":{},\"headers\":{},\"url\":\"httpbin.org\"}";
    }

    String qirdResponse = "+QIRD: " + String(httpResponse.length()) + "\r\n" +
                          httpResponse + "\r\n\r\nOK\r\n";
    mockStream->InjectRxData(qirdResponse);

    // Subsequent QIRD calls return no data
    mockStream->InjectRxData("+QIRD: 0\r\n\r\nOK\r\n");
    mockStream->InjectRxData("+QIRD: 0\r\n\r\nOK\r\n");
    mockStream->InjectRxData("+QIRD: 0\r\n\r\nOK\r\n");
  }
};

TEST_F(AsyncGSMHttpClientTest, HttpGetRequest) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        clearMockStream();

        if (!gsm->init(*mockStream)) {
          throw std::runtime_error("GSM init failed");
        }

        setupConnectionResponses();
        setupQisendResponses(25);
        setupHttpResponse(false);

        log_i("[Test] Starting HTTP GET request...");

        int connectionResult = httpClient->get("/get");

        log_i("[Test] HTTP GET connection result: %d", connectionResult);

        if (connectionResult != 0) {
          throw std::runtime_error("HTTP connection failed");
        }

        vTaskDelay(pdMS_TO_TICKS(500));

        int statusCode = httpClient->responseStatusCode();
        log_i("[Test] HTTP status code: %d", statusCode);

        if (statusCode != 200) {
          throw std::runtime_error("Expected HTTP 200, got: " +
                                   String(statusCode));
        }

        String responseBody = httpClient->responseBody();
        log_i("[Test] Response body: %s", responseBody.c_str());

        if (responseBody.indexOf("httpbin.org") == -1) {
          throw std::runtime_error("Response validation failed");
        }

        std::string sentData = mockStream->GetTxData();

        if (sentData.find("AT+QIOPEN") == std::string::npos) {
          throw std::runtime_error("Missing QIOPEN command");
        }

        if (sentData.find("GET /get HTTP/1.1") == std::string::npos) {
          throw std::runtime_error("Missing HTTP GET request");
        }

        log_i("[Test] HTTP GET test completed successfully");
      },
      "HttpGetTest", 8192, 2, 15000);

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncGSMHttpClientTest, HttpPostWithJson) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        clearMockStream();

        if (!gsm->init(*mockStream)) {
          throw std::runtime_error("GSM init failed");
        }

        String jsonPayload = "{\"temperature\":23.5,\"humidity\":65.2}";

        setupConnectionResponses();
        setupQisendResponses(30);
        setupHttpResponse(true);

        log_i("[Test] Starting HTTP POST request...");

        // Corrected order for HttpClient calls
        httpClient->beginRequest();
        httpClient->post("/post");
        httpClient->sendHeader("Content-Type", "application/json");
        httpClient->sendHeader("Content-Length", jsonPayload.length());
        httpClient->beginBody();
        httpClient->print(jsonPayload);
        httpClient->endRequest();

        vTaskDelay(pdMS_TO_TICKS(500));

        int statusCode = httpClient->responseStatusCode();
        log_i("[Test] HTTP status code: %d", statusCode);

        if (statusCode != 201) {
          throw std::runtime_error("Expected HTTP 201, got: " +
                                   String(statusCode));
        }

        String responseBody = httpClient->responseBody();
        log_i("[Test] Response body: %s", responseBody.c_str());

        if (responseBody.indexOf("created") == -1) {
          throw std::runtime_error("POST response validation failed");
        }

        std::string sentData = mockStream->GetTxData();

        if (sentData.find("POST /post HTTP/1.1") == std::string::npos) {
          throw std::runtime_error("Missing HTTP POST request");
        }

        if (sentData.find(jsonPayload.c_str()) == std::string::npos) {
          throw std::runtime_error("Missing JSON payload");
        }

        log_i("[Test] HTTP POST test completed successfully");
      },
      "HttpPostTest", 8192, 2, 15000);

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncGSMHttpClientTest, HttpConnectionFailure) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        clearMockStream();

        if (!gsm->init(*mockStream)) {
          throw std::runtime_error("GSM init failed");
        }

        mockStream->InjectRxData("OK\r\n");
        mockStream->InjectRxData("OK\r\n");
        mockStream->InjectRxData("OK\r\n");
        mockStream->InjectRxData("+QIOPEN: 0,1\r\n");

        log_i("[Test] Testing connection failure...");

        int connectionResult = httpClient->get("/test");

        log_i("[Test] Connection result: %d", connectionResult);

        if (connectionResult == 0) {
          throw std::runtime_error("Expected connection failure");
        }

        std::string sentData = mockStream->GetTxData();
        if (sentData.find("AT+QIOPEN") == std::string::npos) {
          throw std::runtime_error("Connection attempt not found");
        }

        log_i("[Test] Connection failure test completed successfully");
      },
      "ConnectionFailureTest", 8192, 2, 10000);

  EXPECT_TRUE(testResult);
}

FREERTOS_TEST_MAIN()

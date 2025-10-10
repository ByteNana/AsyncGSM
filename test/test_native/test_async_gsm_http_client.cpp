#include <AsyncGSM.h>
#include <HttpClient.h>
#include <atomic>
#include <string>

#include "common/common.h"
#include "common/responder.h"

using ::testing::NiceMock;

// Test-only client subclass that coordinates with AsyncATHandler promises
class TestAsyncGSM : public AsyncGSM {
public:
  using AsyncGSM::AsyncGSM;

  size_t write(const uint8_t *buf, size_t size) override {
    // Push payload into the underlying mock stream so tests can inspect it
    if (!getATHandler().getStream() || size == 0)
      return 0;
    getATHandler().getStream()->write(buf, size);
    getATHandler().getStream()->flush();
    return size;
  }
  size_t write(uint8_t c) override { return write(&c, 1); }
};

class HttpClientUsageTest : public FreeRTOSTest {
protected:
  TestAsyncGSM *gsm{nullptr};
  NiceMock<MockStream> *mock{nullptr};

  void SetUp() override {
    FreeRTOSTest::SetUp();
    gsm = new TestAsyncGSM();
    mock = new NiceMock<MockStream>();
    mock->SetupDefaults();
  }
  void TearDown() override {
    if (gsm)
      delete gsm;
    if (mock)
      delete mock;
    FreeRTOSTest::TearDown();
  }
};

// Responder helpers are in common/responder.h

TEST_F(HttpClientUsageTest, GetRequestReturnsBody) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(gsm->init(*mock));

        std::atomic<bool> done{false};
        std::string captured;
        startTcpResponderCapturing(mock, &done, &captured, false);

        // Prepare incoming HTTP response via modem's read URC
        const std::string httpPayload =
            "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello\r\n\r\n";
        const std::string inj = std::string("+QIRD: ") +
                                std::to_string(httpPayload.size()) + "\r\n" +
                                httpPayload + "OK\r\n";
        scheduleInject(mock, 200, inj);

        HttpClient http(*gsm, "example.com", 80);
        int err = http.get("/");
        ASSERT_EQ(err, 0);
        int status = http.responseStatusCode();
        EXPECT_EQ(status, 200);

        String body = http.responseBody();
        EXPECT_EQ(body, String("hello"));

        done = true;
        vTaskDelay(pdMS_TO_TICKS(20));
      },
      "HttpClientGET", 8192, 3, 8000);
  EXPECT_TRUE(ok);
}

TEST_F(HttpClientUsageTest, ConnectFailurePropagatesToHttpClient) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(gsm->init(*mock));
        std::atomic<bool> done{false};
        startTcpResponder(mock, &done, true); // force QIOPEN failure

        HttpClient http(*gsm, "bad.host", 80);
        int status = http.get("/");
        EXPECT_LT(status, 0); // HttpClient should report an error

        done = true;
        vTaskDelay(pdMS_TO_TICKS(20));
      },
      "HttpClientFail", 8192, 3, 5000);
  EXPECT_TRUE(ok);
}

TEST_F(HttpClientUsageTest, SendsRequestAndReadsResponse) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(gsm->init(*mock));

        std::atomic<bool> done{false};
        std::string captured;
        startTcpResponderCapturing(mock, &done, &captured, false);

        // Provide an HTTP/1.1 200 OK response via +QIRD
        const std::string httpPayload =
            "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello\r\n\r\n";
        const std::string inj = std::string("+QIRD: ") +
                                std::to_string(httpPayload.size()) + "\r\n" +
                                httpPayload + "OK\r\n";
        scheduleInject(mock, 200, inj);

        // Send GET request
        HttpClient http(*gsm, "example.com", 80);
        int err = http.get("/");
        ASSERT_EQ(err, 0);

        // Capture what the client wrote as the HTTP request
        vTaskDelay(pdMS_TO_TICKS(20)); // allow writer to flush
        ASSERT_FALSE(captured.empty());
        // Verify start line and essential headers
        EXPECT_NE(captured.find("GET / HTTP/1.1\r\n"), std::string::npos);
        EXPECT_NE(captured.find("Host: example.com"), std::string::npos);

        // Verify response parsing
        int status = http.responseStatusCode();
        EXPECT_EQ(status, 200);
        String body = http.responseBody();
        EXPECT_EQ(body, String("hello"));

        done = true;
        vTaskDelay(pdMS_TO_TICKS(20));
      },
      "HttpSendRead", 8192, 3, 8000);
  EXPECT_TRUE(ok);
}

TEST_F(HttpClientUsageTest, PostSendsBodyAndReadsResponse) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(gsm->init(*mock));

        std::atomic<bool> done{false};
        std::string captured;
        startTcpResponderCapturing(mock, &done, &captured, false);

        // Provide a 200 OK response for POST
        const std::string respBody = "ok";
        const std::string httpPayload =
            std::string("HTTP/1.1 200 OK\r\nContent-Length: ") +
            std::to_string(respBody.size()) + "\r\n\r\n" + respBody +
            "\r\n\r\n";
        const std::string inj = std::string("+QIRD: ") +
                                std::to_string(httpPayload.size()) + "\r\n" +
                                httpPayload + "OK\r\n";
        scheduleInject(mock, 200, inj);

        // Send POST request with JSON body
        const char *body = "{\"x\":1}";
        const char *ctype = "application/json";
        HttpClient http(*gsm, "example.com", 80);
        int err = http.post("/api", ctype, body);
        ASSERT_EQ(err, 0);

        // Verify sent request
        vTaskDelay(pdMS_TO_TICKS(30));
        ASSERT_FALSE(captured.empty());
        EXPECT_NE(captured.find("POST /api HTTP/1.1\r\n"), std::string::npos);
        EXPECT_NE(captured.find("Host: example.com"), std::string::npos);
        EXPECT_NE(captured.find("Content-Type: application/json"),
                  std::string::npos);
        EXPECT_NE(captured.find(std::string("Content-Length: ") +
                                std::to_string(strlen(body))),
                  std::string::npos);
        EXPECT_NE(captured.find(body), std::string::npos);

        // Verify response handling
        int status = http.responseStatusCode();
        EXPECT_EQ(status, 200);
        String rb = http.responseBody();
        EXPECT_EQ(rb, String(respBody.c_str()));

        done = true;
        vTaskDelay(pdMS_TO_TICKS(20));
      },
      "HttpPostSendRead", 8192, 3, 8000);
  EXPECT_TRUE(ok);
}

FREERTOS_TEST_MAIN()

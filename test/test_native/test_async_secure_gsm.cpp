#include <AsyncGSM.h>
#include <AsyncSecureGSM.h>

#include <atomic>
#include <string>

#include "common/common.h"
#include "common/responder.h"

using ::testing::NiceMock;

// Minimal SSL responder handling QSSLCFG/QSSLOPEN/QSSLSEND/QSSLCLOSE and file ops
static void startSslResponderCapturing(
    NiceMock<MockStream> *s, std::atomic<bool> *done, std::string *captured) {
  auto responder = [](void *pv) {
    auto *ctx =
        static_cast<std::tuple<NiceMock<MockStream> *, std::atomic<bool> *, std::string *> *>(pv);
    auto *s = std::get<0>(*ctx);
    auto *done = std::get<1>(*ctx);
    auto *cap = std::get<2>(*ctx);

    std::string acc;
    TickType_t start = xTaskGetTickCount();
    const TickType_t maxTicks = pdMS_TO_TICKS(10000);
    while (!done->load()) {
      if ((xTaskGetTickCount() - start) > maxTicks) break;

      std::string chunk = DrainTx(s);
      if (!chunk.empty()) acc += chunk;

      size_t pos;
      while ((pos = acc.find("\r\n")) != std::string::npos) {
        std::string cmd = acc.substr(0, pos);
        acc.erase(0, pos + 2);
        auto starts_with = [&](const char *p) { return cmd.rfind(p, 0) == 0; };

        // Allow all SSL config commands
        if (starts_with("AT+QSSLCFG=")) {
          InjectRx(s, "OK\r\n");
          if (cap) { (*cap) += cmd + "\r\n"; }
          continue;
        }
        if (starts_with("AT+QSSLOPEN=")) {
          InjectRx(s, "OK\r\n");
          InjectRx(s, "+QSSLOPEN: 0,0\r\n");
          if (cap) { (*cap) += cmd + "\r\n"; }
          continue;
        }
        if (starts_with("AT+QSSLSEND=")) {
          InjectRx(s, ">\r\n");
          vTaskDelay(pdMS_TO_TICKS(1));
          InjectRx(s, "OK\r\n");
          InjectRx(s, "SEND OK\r\n");
          if (cap) { (*cap) += cmd + "\r\n"; }
          continue;
        }
        if (starts_with("AT+QSSLCLOSE")) {
          InjectRx(s, "OK\r\n");
          if (cap) { (*cap) += cmd + "\r\n"; }
          continue;
        }
        // File ops used by setCACert
        if (starts_with("AT+QFLST=")) {
          // No matches -> force upload path
          InjectRx(s, "OK\r\n");
          if (cap) { (*cap) += cmd + "\r\n"; }
          continue;
        }
        if (starts_with("AT+QFUPL=")) {
          InjectRx(s, "CONNECT\r\n");
          vTaskDelay(pdMS_TO_TICKS(1));
          InjectRx(s, "OK\r\n");
          if (cap) { (*cap) += cmd + "\r\n"; }
          continue;
        }

        // Capture any other lines for debugging
        if (cap) { (*cap) += cmd + "\r\n"; }
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    delete ctx;
    vTaskDelete(nullptr);
  };
  auto *ctx =
      new std::tuple<NiceMock<MockStream> *, std::atomic<bool> *, std::string *>(s, done, captured);
  xTaskCreate(responder, "SSL_RESP", configMINIMAL_STACK_SIZE * 4, ctx, 1, nullptr);
}

class AsyncSecureGsmTest : public FreeRTOSTest {
 protected:
  AsyncSecureGSM *gsm{nullptr};
  NiceMock<MockStream> *mock{nullptr};

  void SetUp() override {
    FreeRTOSTest::SetUp();
    gsm = new AsyncSecureGSM();
    mock = new NiceMock<MockStream>();
    mock->SetupDefaults();
  }
  void TearDown() override {
    if (gsm) {
      gsm->context().end();
      vTaskDelay(pdMS_TO_TICKS(80));
      delete gsm;
    }
    if (mock) delete mock;
    FreeRTOSTest::TearDown();
  }
};

TEST_F(AsyncSecureGsmTest, SecureConnectAndSendUsesSSLPath) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(gsm->context().begin(*mock));

        std::atomic<bool> done{false};
        std::string captured;
        startSslResponderCapturing(mock, &done, &captured);

        // Connect to TLS port and send some data
        EXPECT_TRUE(gsm->connect("example.com", 443));
        const char *msg = "ping";
        size_t n = gsm->write(reinterpret_cast<const uint8_t *>(msg), strlen(msg));
        EXPECT_EQ(n, strlen(msg));

        // Ensure SSL send command path was used
        EXPECT_NE(captured.find("AT+QSSLSEND=0,4"), std::string::npos);

        done = true;
        vTaskDelay(pdMS_TO_TICKS(50));
      },
      "SecureConnectSend", 8192, 3, 6000);
  EXPECT_TRUE(ok);
}

TEST_F(AsyncSecureGsmTest, SetCACertUploadsOnceAndCaches) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(gsm->context().begin(*mock));
        std::atomic<bool> done{false};
        std::string captured;
        startSslResponderCapturing(mock, &done, &captured);

        const char *pem = "-----BEGIN CERTIFICATE-----\nABC\n-----END CERTIFICATE-----\n";

        // First call should QFLST (no match), QFUPL, and configure cacert
        gsm->setCACert(pem);
        vTaskDelay(pdMS_TO_TICKS(10));
        EXPECT_NE(captured.find("AT+QFLST="), std::string::npos);
        EXPECT_NE(captured.find("AT+QFUPL="), std::string::npos);
        EXPECT_NE(captured.find("AT+QSSLCFG=\"cacert\""), std::string::npos);
        size_t before = captured.size();

        // Second call with same cert should be a no-op
        gsm->setCACert(pem);
        vTaskDelay(pdMS_TO_TICKS(10));
        size_t after = captured.size();
        EXPECT_EQ(before, after);

        done = true;
        vTaskDelay(pdMS_TO_TICKS(30));
      },
      "CACertCache", 8192, 3, 6000);
  EXPECT_TRUE(ok);
}

FREERTOS_TEST_MAIN()

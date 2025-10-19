#include <atomic>
#include <string>

#include "AsyncGSM.h"
#include "Stream.h"
#include "common/common.h"
#include "common/responder.h"

using ::testing::NiceMock;

class AsyncGSMGprsTest : public FreeRTOSTest {
 protected:
  AsyncGSM *gsm{nullptr};
  NiceMock<MockStream> *mockStream{nullptr};

  void SetUp() override {
    FreeRTOSTest::SetUp();
    gsm = new AsyncGSM();
    mockStream = new NiceMock<MockStream>();
    mockStream->SetupDefaults();
  }

  void TearDown() override {
    if (gsm) {
      gsm->context().end();
      vTaskDelay(pdMS_TO_TICKS(80));
      delete gsm;
    }
    if (mockStream) delete mockStream;
    FreeRTOSTest::TearDown();
  }
};

static void startGprsResponder(NiceMock<MockStream> *mockStream, std::atomic<bool> *done) {
  auto responder = [](void *pv) {
    auto *ctx = static_cast<std::tuple<NiceMock<MockStream> *, std::atomic<bool> *> *>(pv);
    auto *s = std::get<0>(*ctx);
    auto *done = std::get<1>(*ctx);

    // Signal network registered (URC) so setPDPContext won't loop.
    s->InjectRxData("+CREG: 0,1\r\n");

    std::string buf;
    while (!done->load()) {
      std::string tx = DrainTx(s);
      if (!tx.empty()) buf += tx;

      size_t pos;
      while ((pos = buf.find("\r\n")) != std::string::npos) {
        std::string cmd = buf.substr(0, pos);
        buf.erase(0, pos + 2);

        auto starts_with = [&](const char *p) { return cmd.rfind(p, 0) == 0; };

        // gprsConnect() path only
        if (cmd == "AT+CREG?") {
          InjectRx(s, "OK\r\n");
          continue;
        }
        if (starts_with("AT+CGDCONT=")) {
          InjectRx(s, "OK\r\n");
          continue;
        }
        if (starts_with("AT+QIDEACT=1")) {
          InjectRx(s, "OK\r\n");
          continue;
        }
        if (starts_with("AT+QICSGP=")) {
          InjectRx(s, "OK\r\n");
          continue;
        }
        if (cmd == "AT+CGATT?") {
          InjectRx(s, "+CGATT: 1\r\nOK\r\n");
          continue;
        }
        if (cmd == "AT+QIACT=1") {
          InjectRx(s, "OK\r\n");
          continue;
        }
        if (cmd == "AT+CGACT=1,1") {
          // Suppress informative +CGACT URC in tests to avoid races; send only
          // OK
          InjectRx(s, "OK\r\n");
          continue;
        }
        if (cmd == "AT+QIACT?") {
          InjectRx(s, "+QIACT: 1,1\r\nOK\r\n");
          continue;
        }

        // Default OK for any other command (harmless)
        if (starts_with("AT+")) {
          InjectRx(s, "OK\r\n");
          continue;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    vTaskDelete(nullptr);
  };

  auto *ctx = new std::tuple<NiceMock<MockStream> *, std::atomic<bool> *>(mockStream, done);
  xTaskCreate(responder, "GPRS_RESP", configMINIMAL_STACK_SIZE * 4, ctx, 1, nullptr);
}

TEST_F(AsyncGSMGprsTest, GprsConnect_Succeeds_WithAPNUserPwd) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(gsm->context().begin(*mockStream));

        // Pretend we are already registered to skip wait loop
        gsm->context().modem().URCState.creg.store(RegStatus::REG_OK_HOME);

        std::atomic<bool> done{false};
        startGprsResponder(mockStream, &done);

        bool res = gsm->context().modem().gprsConnect("apn", "user", "pwd");
        done = true;
        vTaskDelay(pdMS_TO_TICKS(120));
        ASSERT_TRUE(res);
      },
      "GprsConnectOK", 8192, 2, 8000);
  EXPECT_TRUE(ok);
}

TEST_F(AsyncGSMGprsTest, GprsConnect_Succeeds_WithAPNOnly) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(gsm->context().begin(*mockStream));

        gsm->context().modem().URCState.creg.store(RegStatus::REG_OK_HOME);

        std::atomic<bool> done{false};
        startGprsResponder(mockStream, &done);

        bool res = gsm->context().modem().gprsConnect("apn");
        done = true;
        vTaskDelay(pdMS_TO_TICKS(120));
        ASSERT_TRUE(res);
      },
      "GprsConnectAPNOnly", 8192, 2, 8000);
  EXPECT_TRUE(ok);
}

FREERTOS_TEST_MAIN()

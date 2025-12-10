#include <AsyncGSM.h>
#include <AsyncMqttGSM.h>
#include <modules/EG915/EG915.h>

#include <atomic>
#include <string>

#include "common/responder.h"

using ::testing::NiceMock;

static void startMqttCfgResponder(
    NiceMock<MockStream> *s, std::atomic<bool> *done, std::string *capture = nullptr) {
  auto responder = [](void *pv) {
    using Ctx =
        std::tuple<NiceMock<MockStream> *, std::atomic<bool> *, std::string *, std::atomic<bool> *>;
    auto *ctx = static_cast<Ctx *>(pv);
    auto *s = std::get<0>(*ctx);
    auto *done = std::get<1>(*ctx);
    auto *cap = std::get<2>(*ctx);
    auto *started = std::get<3>(*ctx);

    std::string acc;
    TickType_t start = xTaskGetTickCount();
    const TickType_t maxTicks = pdMS_TO_TICKS(10000);
    if (started) started->store(true);
    while (!done->load()) {
      if ((xTaskGetTickCount() - start) > maxTicks) break;

      std::string chunk = DrainTx(s);
      if (!chunk.empty()) acc += chunk;

      size_t pos;
      while ((pos = acc.find("\r\n")) != std::string::npos) {
        std::string cmd = acc.substr(0, pos);
        acc.erase(0, pos + 2);
        if (cap) {
          (*cap) += cmd;
          (*cap) += "\r\n";
        }
        auto starts_with = [&](const char *p) { return cmd.rfind(p, 0) == 0; };

        // Accept any QMTCFG configuration
        if (starts_with("AT+QMTCFG=")) {
          InjectRx(s, "OK\r\n");
          continue;
        }
        // Accept empty lines used for expect chains
        if (cmd.empty()) {
          InjectRx(s, "OK\r\n");
          continue;
        }
        // Generic OK for other AT+ commands
        if (starts_with("AT+")) {
          InjectRx(s, "OK\r\n");
          continue;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    delete started;
    delete ctx;
    vTaskDelete(nullptr);
  };
  auto *started = new std::atomic<bool>(false);
  auto *_ctx = new std::tuple<
      NiceMock<MockStream> *, std::atomic<bool> *, std::string *, std::atomic<bool> *>(
      s, done, capture, started);
  xTaskCreate(responder, "MQTT_CFG_RESP", configMINIMAL_STACK_SIZE * 4, _ctx, 1, nullptr);
  TickType_t t0 = xTaskGetTickCount();
  while (!started->load() && (xTaskGetTickCount() - t0) < pdMS_TO_TICKS(50)) {
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

class MqttURCTest : public FreeRTOSTest {
 protected:
  AsyncGSM *gsm{nullptr};
  NiceMock<MockStream> *mock{nullptr};
  AsyncMqttGSM *mqtt{nullptr};
  GSMContext *ctx{};
  AsyncEG915U module;

  void SetUp() override {
    FreeRTOSTest::SetUp();
    mock = new NiceMock<MockStream>();
    mock->SetupDefaults();
    ctx = new GSMContext();
    ASSERT_TRUE(ctx->begin(*mock, module));
    gsm = new AsyncGSM(*ctx);
    mqtt = new AsyncMqttGSM(*ctx);
  }
  void TearDown() override {
    // Ensure background tasks are stopped before destroying stream/context
    if (ctx) {
      ctx->end();
      vTaskDelay(pdMS_TO_TICKS(80));
    }
    if (gsm) delete gsm;
    if (mock) delete mock;
    if (mqtt) delete mqtt;
    if (ctx) delete ctx;
    FreeRTOSTest::TearDown();
  }
};

TEST_F(MqttURCTest, QMTRECV_HeaderOnly_RequestsFetch) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        // Initialize MQTT helper (sends QMTCFG commands)
        std::atomic<bool> done{false};
        std::string cap;
        startMqttCfgResponder(mock, &done, &cap);
        ASSERT_TRUE(mqtt->init());

        // Clear any previous TX
        (void)mock->GetTxData();

        // Inject a header-only QMTRECV URC to request fetch
        InjectRx(mock, "\r\n+QMTRECV: 1,7\r\n");
        vTaskDelay(pdMS_TO_TICKS(30));

        // Check captured outgoing commands from responder
        EXPECT_NE(cap.find("AT+QMTRECV=1,7"), std::string::npos);

        done = true;
        vTaskDelay(pdMS_TO_TICKS(50));
      },
      "MQTT_QMTRECV_FETCH", 8192, 3, 8000);
  EXPECT_TRUE(ok);
}

TEST_F(MqttURCTest, QMTRECV_WithTopicPayload_QueuesAndCallback) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        std::atomic<bool> done{false};
        startMqttCfgResponder(mock, &done);
        ASSERT_TRUE(mqtt->init());

        // Mark MQTT as connected so loop processes queue
        gsm->context().modem().mqtt().setState(MqttConnectionState::CONNECTED);

        // Capture callback
        std::atomic<bool> called{false};
        String gotTopic;
        String gotPayload;
        mqtt->setCallback([&](char *topic, uint8_t *payload, unsigned int len) {
          called = true;
          gotTopic = String(topic);
          gotPayload = String(reinterpret_cast<char *>(payload)).substring(0, len);
        });

        // Inject a QMTRECV with topic and payload
        InjectRx(mock, "\r\n+QMTRECV: 1,9,\"/foo\",\"bar\"\r\nOK\r\n");

        // Let the queue populate and the loop process
        for (int i = 0; i < 15 && !called.load(); ++i) {
          mqtt->loop();
          vTaskDelay(pdMS_TO_TICKS(20));
        }

        EXPECT_TRUE(called.load());
        EXPECT_EQ(gotTopic, String("/foo"));
        EXPECT_EQ(gotPayload, String("bar"));

        done = true;
        vTaskDelay(pdMS_TO_TICKS(50));
      },
      "MQTT_QMTRECV_CB", 8192, 3, 8000);
  EXPECT_TRUE(ok);
}

TEST_F(MqttURCTest, QMTSTAT_SetsDisconnected) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        // Initial state
        gsm->context().modem().mqtt().setState(MqttConnectionState::CONNECTED);
        InjectRx(mock, "\r\n+QMTSTAT: 1,2\r\n");
        vTaskDelay(pdMS_TO_TICKS(20));
        EXPECT_EQ(gsm->context().modem().mqtt().getState(), MqttConnectionState::DISCONNECTED);
      },
      "MQTT_QMTSTAT", 8192, 3, 3000);
  EXPECT_TRUE(ok);
}

TEST_F(MqttURCTest, QIRD_ReadDataTimeout_BreaksLoop) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        InjectRx(mock, "\r\n+QIRD: 100\r\n");
        // Do NOT provide the 100 bytes of data. The onReadData should timeout.
        // The timeout is 5 seconds in EG915.urc.cpp, so we wait a bit longer.
        vTaskDelay(pdMS_TO_TICKS(6000));

        // If the test reaches here, it means onReadData did not hang indefinitely.
        SUCCEED();
      },
      "QIRD_READ_TIMEOUT", 8192, 3, 8000);
  EXPECT_TRUE(ok);
}

FREERTOS_TEST_MAIN()

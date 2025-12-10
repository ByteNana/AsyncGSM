#include <AsyncSecureMqttGSM.h>
#include <modules/EG915/EG915.h>

#include <atomic>
#include <string>

#include "common/common.h"
#include "common/responder.h"

using ::testing::NiceMock;

// Responder for MQTT over TLS: handles QFLST/QFUPL/QSSLCFG and basic MQTT open/conn/pub
static void startMqttsResponder(
    NiceMock<MockStream>* s, std::atomic<bool>* done, std::string* capture = nullptr) {
  auto responder = [](void* pv) {
    auto* ctx =
        static_cast<std::tuple<NiceMock<MockStream>*, std::atomic<bool>*, std::string*>*>(pv);
    auto* s = std::get<0>(*ctx);
    auto* done = std::get<1>(*ctx);
    auto* cap = std::get<2>(*ctx);

    std::string acc;
    TickType_t start = xTaskGetTickCount();
    const TickType_t maxTicks = pdMS_TO_TICKS(10000);
    bool lastOpen = false;
    bool lastConn = false;
    bool lastPub = false;

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
        auto sw = [&](const char* p) { return cmd.rfind(p, 0) == 0; };

        // TLS-related configuration
        if (sw("AT+QFLST=")) {
          InjectRx(s, "OK\r\n");
          continue;
        }
        if (sw("AT+QFUPL=")) {
          InjectRx(s, "CONNECT\r\n");
          vTaskDelay(pdMS_TO_TICKS(1));
          InjectRx(s, "OK\r\n");
          continue;
        }
        if (sw("AT+QSSLCFG=")) {
          InjectRx(s, "OK\r\n");
          continue;
        }

        // Generic MQTT configuration (version, keepalive, etc.)
        if (sw("AT+QMTCFG=")) {
          InjectRx(s, "OK\r\n");
          continue;
        }

        // MQTT open/connect
        if (sw("AT+QMTOPEN=")) {
          InjectRx(s, "OK\r\n");
          lastOpen = true;
          continue;
        }
        if (sw("AT+QMTCONN=")) {
          InjectRx(s, "OK\r\n");
          lastConn = true;
          continue;
        }

        // Publish extended
        if (sw("AT+QMTPUBEX=")) {
          InjectRx(s, ">\r\n");
          lastPub = true;
          continue;
        }
        if (!cmd.empty() && cmd[0] != 'A') {
          if (lastPub) {
            InjectRx(s, "+QMTPUBEX: 1,1,0\r\n");
            lastPub = false;
            continue;
          }
          InjectRx(s, "OK\r\n");
          continue;
        }

        // Final acks emitted after an empty command
        if (cmd.empty()) {
          if (lastOpen) {
            InjectRx(s, "+QMTOPEN: 1,0\r\n");
            lastOpen = false;
            continue;
          }
          if (lastConn) {
            InjectRx(s, "+QMTCONN: 1,0,0\r\n");
            lastConn = false;
            continue;
          }
          if (lastPub) {
            InjectRx(s, "+QMTPUBEX: 1,1,0\r\n");
            lastPub = false;
            continue;
          }
          InjectRx(s, "OK\r\n");
          continue;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    delete ctx;
    vTaskDelete(nullptr);
  };
  auto* ctx =
      new std::tuple<NiceMock<MockStream>*, std::atomic<bool>*, std::string*>(s, done, capture);
  xTaskCreate(responder, "MQTTS_RESP", configMINIMAL_STACK_SIZE * 4, ctx, 1, nullptr);
}

class MqttsTest : public FreeRTOSTest {
 protected:
  GSMContext* ctx{nullptr};
  NiceMock<MockStream>* mock{nullptr};
  AsyncSecureMqttGSM* mqtts{nullptr};
  AsyncEG915U module;

  void SetUp() override {
    FreeRTOSTest::SetUp();
    ctx = new GSMContext();
    mock = new NiceMock<MockStream>();
    mock->SetupDefaults();
    mqtts = new AsyncSecureMqttGSM(*ctx);
  }
  void TearDown() override {
    if (ctx) {
      ctx->end();
      vTaskDelay(pdMS_TO_TICKS(50));
      delete ctx;
    }
    if (mqtts) delete mqtts;
    if (mock) delete mock;
    FreeRTOSTest::TearDown();
  }
};

// Per docs: configure SSL before connecting (we set CA cert first)
TEST_F(MqttsTest, ConfigureSslBeforeConnect) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(ctx->begin(*mock, module));
        std::atomic<bool> done{false};
        std::string cap;
        startMqttsResponder(mock, &done, &cap);

        // Provide a dummy PEM; we only need the commands to be sent
        const char* pem = "-----BEGIN CERTIFICATE-----\nABC\n-----END CERTIFICATE-----\n";
        mqtts->setCACert(pem);

        // Initialize generic MQTT config; TLS was configured beforehand by setCACert
        ASSERT_TRUE(mqtts->init());

        // Basic sanity: we saw QMTCFG and QSSLCFG commands in the capture
        EXPECT_NE(cap.find("AT+QMTCFG="), std::string::npos);
        EXPECT_NE(cap.find("AT+QSSLCFG=\"cacert\""), std::string::npos);

        done = true;
        vTaskDelay(pdMS_TO_TICKS(50));
      },
      "MQTTS_Config", 8192, 3, 8000);
  EXPECT_TRUE(ok);
}

TEST_F(MqttsTest, ConnectPublishOverTls) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(ctx->begin(*mock, module));
        std::atomic<bool> done{false};
        startMqttsResponder(mock, &done);

        const char* pem = "-----BEGIN CERTIFICATE-----\nXYZ\n-----END CERTIFICATE-----\n";
        mqtts->setCACert(pem);
        ASSERT_TRUE(mqtts->init());

        mqtts->setServer("broker", 8883);
        ASSERT_TRUE(mqtts->connect("id", "user", "pass"));

        const char* topic = "/t";
        const uint8_t payload[] = {'o', 'k'};
        ASSERT_TRUE(mqtts->publish(topic, payload, 2));

        done = true;
        vTaskDelay(pdMS_TO_TICKS(50));
      },
      "MQTTS_CONN_PUB", 8192, 3, 8000);
  EXPECT_TRUE(ok);
}

FREERTOS_TEST_MAIN()
#include <AsyncGSM.h>
#include <AsyncMqttGSM.h>
#include <AsyncSecureGSM.h>

#include <atomic>
#include <string>

#include "common/common.h"
#include "common/responder.h"

using ::testing::NiceMock;

class GsmContextSharedTest : public FreeRTOSTest {};

// Minimal MQTT config responder that ACKs QMTCFG/AT+ commands
static void startMqttCfgResponder(NiceMock<MockStream> *s, std::atomic<bool> *done) {
  auto responder = [](void *pv) {
    using Ctx = std::tuple<NiceMock<MockStream> *, std::atomic<bool> *, std::atomic<bool> *>;
    auto *ctx = static_cast<Ctx *>(pv);
    auto *s = std::get<0>(*ctx);
    auto *done = std::get<1>(*ctx);
    auto *started = std::get<2>(*ctx);
    std::string acc;
    TickType_t start = xTaskGetTickCount();
    const TickType_t maxTicks = pdMS_TO_TICKS(8000);
    if (started) started->store(true);
    while (!done->load()) {
      if ((xTaskGetTickCount() - start) > maxTicks) break;
      std::string chunk = DrainTx(s);
      if (!chunk.empty()) acc += chunk;
      size_t pos;
      while ((pos = acc.find("\r\n")) != std::string::npos) {
        std::string cmd = acc.substr(0, pos);
        acc.erase(0, pos + 2);
        auto starts_with = [&](const char *p) { return cmd.rfind(p, 0) == 0; };
        // Only acknowledge MQTT-related configuration commands here, not generic AT+
        if (starts_with("AT+QMTCFG=") || starts_with("AT+QMT")) {
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
  auto *ctx = new std::tuple<NiceMock<MockStream> *, std::atomic<bool> *, std::atomic<bool> *>(
      s, done, started);
  // Slightly higher priority to ensure it starts promptly
  xTaskCreate(responder, "MQTT_CFG", configMINIMAL_STACK_SIZE * 4, ctx, 3, nullptr);
  TickType_t t0 = xTaskGetTickCount();
  while (!started->load() && (xTaskGetTickCount() - t0) < pdMS_TO_TICKS(50)) {
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

TEST_F(GsmContextSharedTest, SharedContextByInstances) {
  bool ok = runInFreeRTOSTask(
      []() {
        GSMContext ctx;
        AsyncGSM a(ctx);
        AsyncSecureGSM b(ctx);
        AsyncMqttGSM m(ctx);

        NiceMock<MockStream> mock;  // stack-allocated to avoid leaks on early failures
        mock.SetupDefaults();
        ASSERT_TRUE(ctx.begin(mock));

        std::atomic<bool> done{false};
        // Use a single responder that handles TCP and OKs generic AT+ (incl. QMTCFG)
        startTcpResponder(&mock, &done, /*failOpen=*/false);
        // Give responder time to start before first AT command
        vTaskDelay(pdMS_TO_TICKS(30));

        EXPECT_EQ(a.connect("example.com", 80), 1);
        vTaskDelay(pdMS_TO_TICKS(50));
        // Ensure only AsyncGSM instance is connected (TCP)
        EXPECT_TRUE(a.connected());
        EXPECT_TRUE(b.connected());

        a.stop();
        // Wait up to 500ms for URC to mark disconnected
        {
          TickType_t start = xTaskGetTickCount();
          while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(500)) {
            if (!a.connected() && !b.connected()) break;
            vTaskDelay(pdMS_TO_TICKS(10));
          }
        }
        EXPECT_FALSE(a.connected());
        EXPECT_FALSE(b.connected());

        // MQTT helper should init successfully sharing same context
        bool inited = m.init();
        EXPECT_TRUE(inited);
        if (!inited) {
          done = true;
          vTaskDelay(pdMS_TO_TICKS(50));
          ctx.end();
          vTaskDelay(pdMS_TO_TICKS(50));
          return;
        }

        done = true;
        ctx.end();
      },
      "SharedCtxInstances", 8192, 3, 20000);
  EXPECT_TRUE(ok);
}

TEST_F(GsmContextSharedTest, SharedContextByPointerAndMix) {
  bool ok = runInFreeRTOSTask(
      []() {
        auto *ctx = new GSMContext();
        auto *a = new AsyncGSM(*ctx);  // pointer instance

        auto *mock = new NiceMock<MockStream>();
        mock->SetupDefaults();
        ASSERT_TRUE(ctx->begin(*mock));

        std::atomic<bool> done{false};
        startTcpResponder(mock, &done, /*failOpen=*/false);
        vTaskDelay(pdMS_TO_TICKS(50));

        // Limit scope of b so it is destroyed before we delete ctx
        {
          AsyncSecureGSM b(*ctx);  // stack instance

          EXPECT_EQ(a->connect("example.com", 80), 1);
          vTaskDelay(pdMS_TO_TICKS(50));
          EXPECT_TRUE(a->connected());
          EXPECT_TRUE(b.connected());

          a->stop();
          vTaskDelay(pdMS_TO_TICKS(50));
          EXPECT_FALSE(a->connected());
          EXPECT_FALSE(b.connected());
        }

        done = true;
        vTaskDelay(pdMS_TO_TICKS(80));
        // Stop AT reader before destroying the stream, then allow shutdown
        ctx->end();
        vTaskDelay(pdMS_TO_TICKS(80));
        delete a;
        delete ctx;
        delete mock;
      },
      "SharedCtxPtrMix", 8192, 3, 20000);
  EXPECT_TRUE(ok);
}

FREERTOS_TEST_MAIN()

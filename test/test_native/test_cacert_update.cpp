#include <AsyncGSM.h>

#include <atomic>

#include "common/responder.h"

using ::testing::NiceMock;

// Simple responder that returns a match for a specific QFLST pattern,
// and no matches otherwise.
static void startListResponder(
    NiceMock<MockStream> *s, std::atomic<bool> *done, const char *matchName, size_t matchSize) {
  auto responder = [](void *pv) {
    auto *ctx =
        static_cast<std::tuple<NiceMock<MockStream> *, std::atomic<bool> *, std::string, size_t> *>(
            pv);
    auto *s = std::get<0>(*ctx);
    auto *done = std::get<1>(*ctx);
    std::string name = std::get<2>(*ctx);
    size_t fsize = std::get<3>(*ctx);

    std::string acc;
    TickType_t start = xTaskGetTickCount();
    const TickType_t maxTicks = pdMS_TO_TICKS(5000);
    while (!done->load()) {
      if ((xTaskGetTickCount() - start) > maxTicks) break;
      std::string chunk = DrainTx(s);
      if (!chunk.empty()) acc += chunk;

      size_t pos;
      while ((pos = acc.find("\r\n")) != std::string::npos) {
        std::string line = acc.substr(0, pos);
        acc.erase(0, pos + 2);
        if (line.rfind("AT+QFLST=\"", 0) == 0) {
          // Extract requested pattern
          size_t q1 = line.find('"');
          size_t q2 = line.find('"', q1 + 1);
          std::string pattern = line.substr(q1 + 1, q2 - (q1 + 1));
          if (pattern == name) {
            InjectRx(
                s, String("+QFLST: \"") + name.c_str() + "\"," + String((unsigned long)fsize) +
                       "\r\n");
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
  auto *ctx = new std::tuple<NiceMock<MockStream> *, std::atomic<bool> *, std::string, size_t>(
      s, done, matchName ? matchName : std::string(""), matchSize);
  xTaskCreate(responder, "LIST_RESP", configMINIMAL_STACK_SIZE * 4, ctx, 1, nullptr);
}

class FindFileTest : public FreeRTOSTest {
 protected:
  AsyncGSM *gsm{nullptr};
  NiceMock<MockStream> *mock{nullptr};
  std::atomic<bool> done{false};

  void SetUp() override {
    FreeRTOSTest::SetUp();
    gsm = new AsyncGSM();
    mock = new NiceMock<MockStream>();
    mock->SetupDefaults();
  }
  void TearDown() override {
    done.store(true);
    vTaskDelay(pdMS_TO_TICKS(50));
    if (gsm) {
      gsm->context().end();
      vTaskDelay(pdMS_TO_TICKS(80));
      delete gsm;
    }
    if (mock) delete mock;
    FreeRTOSTest::TearDown();
  }
};

TEST_F(FindFileTest, ReturnsFalseWhenNoMatch) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        startListResponder(mock, &done, "abcdef", 123);
        ASSERT_TRUE(gsm->context().begin(*mock));

        String name;
        size_t size = 0;
        bool found = gsm->context().modem().findUFSFile("notfound", &name, &size);
        EXPECT_FALSE(found);
      },
      "FindNoMatch", 8192, 3, 2000);
  EXPECT_TRUE(ok);
}

TEST_F(FindFileTest, ParsesNameAndSizeOnMatch) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        startListResponder(mock, &done, "deadbeefdeadbeefdeadbeefdeadbeef", 2048);
        ASSERT_TRUE(gsm->context().begin(*mock));

        String name;
        size_t size = 0;
        bool found =
            gsm->context().modem().findUFSFile("deadbeefdeadbeefdeadbeefdeadbeef", &name, &size);
        EXPECT_TRUE(found);
        EXPECT_EQ(name, String("deadbeefdeadbeefdeadbeefdeadbeef"));
        EXPECT_EQ(size, (size_t)2048);
      },
      "FindMatch", 8192, 3, 2000);
  EXPECT_TRUE(ok);
}

FREERTOS_TEST_MAIN()

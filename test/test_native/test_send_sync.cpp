#include <AsyncGSM.h>
#include <modules/EG915/EG915.h>

#include <atomic>
#include <string>

#include "common/responder.h"

using ::testing::NiceMock;

class SendSyncTest : public FreeRTOSTest {
 protected:
  AsyncGSM *gsm{nullptr};
  NiceMock<MockStream> *mock{nullptr};
  AsyncEG915U module;

  void SetUp() override {
    FreeRTOSTest::SetUp();
    gsm = new AsyncGSM();
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

TEST_F(SendSyncTest, OkResponse) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(gsm->context().begin(*mock, module));

        String resp;
        // Schedule an OK after the command is sent
        scheduleInject(mock, 20, std::string("OK\r\n"));
        bool s = gsm->context().at().sendSync("AT+PING", resp, 300);
        EXPECT_TRUE(s);
      },
      "SendSyncOK", 8192, 3, 2000);
  EXPECT_TRUE(ok);
}

TEST_F(SendSyncTest, ErrorResponse) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(gsm->context().begin(*mock, module));

        // Inject ERROR directly for our command
        InjectRx(mock, "ERROR\r\n");
        String resp;
        bool s = gsm->context().at().sendSync("AT+BAD", resp, 200);
        EXPECT_FALSE(s);
      },
      "SendSyncERR", 8192, 3, 2000);
  EXPECT_TRUE(ok);
}

TEST_F(SendSyncTest, Timeout) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(gsm->context().begin(*mock, module));

        String resp;
        bool s = gsm->context().at().sendSync("AT+NOANSWER", resp, 50);
        EXPECT_FALSE(s);
      },
      "SendSyncTO", 8192, 3, 2000);
  EXPECT_TRUE(ok);
}

FREERTOS_TEST_MAIN()
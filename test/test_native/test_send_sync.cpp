#include <AsyncGSM.h>
#include <atomic>
#include <string>

#include "common/responder.h"

using ::testing::NiceMock;

class SendSyncTest : public FreeRTOSTest {
protected:
  AsyncGSM *gsm{nullptr};
  NiceMock<MockStream> *mock{nullptr};

  void SetUp() override {
    FreeRTOSTest::SetUp();
    gsm = new AsyncGSM();
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

TEST_F(SendSyncTest, OkResponse) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(gsm->init(*mock));

        String resp;
        // Schedule an OK after the command is sent
        scheduleInject(mock, 20, std::string("OK\r\n"));
        bool s = gsm->getATHandler().sendSync("AT+PING", resp, 300);
        EXPECT_TRUE(s);
      },
      "SendSyncOK", 8192, 3, 2000);
  EXPECT_TRUE(ok);
}

TEST_F(SendSyncTest, ErrorResponse) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(gsm->init(*mock));
        // Inject ERROR directly for our command
        InjectRx(mock, "ERROR\r\n");
        String resp;
        bool s = gsm->getATHandler().sendSync("AT+BAD", resp, 200);
        EXPECT_FALSE(s);
      },
      "SendSyncERR", 8192, 3, 2000);
  EXPECT_TRUE(ok);
}

TEST_F(SendSyncTest, Timeout) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(gsm->init(*mock));
        String resp;
        bool s = gsm->getATHandler().sendSync("AT+NOANSWER", resp, 50);
        EXPECT_FALSE(s);
      },
      "SendSyncTO", 8192, 3, 2000);
  EXPECT_TRUE(ok);
}

FREERTOS_TEST_MAIN()

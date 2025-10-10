#include <AsyncGSM.h>
#include <atomic>
#include <string>

#include "common/responder.h"

using ::testing::NiceMock;

class URCRegistrationTest : public FreeRTOSTest {
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

TEST_F(URCRegistrationTest, QIOPEN_URC_SetsConnected) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(gsm->init(*mock));

        // Inject a successful open URC and verify state transitions to
        // CONNECTED
        InjectRx(mock, "\r\n+QIOPEN: 0,0\r\n");
        vTaskDelay(pdMS_TO_TICKS(20));
        EXPECT_EQ(gsm->modem.URCState.isConnected.load(),
                  ConnectionStatus::CONNECTED);
      },
      "URC_QIOPEN", 8192, 2, 3000);
  EXPECT_TRUE(ok);
}

TEST_F(URCRegistrationTest, QIURCRecv_TriggersReadCommand) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(gsm->init(*mock));

        // Clear any previous TX
        (void)mock->GetTxData();
        // Data-ready URC should cause a QIRD command to be sent
        InjectRx(mock, "\r\n+QIURC: \"recv\"\r\n");
        vTaskDelay(pdMS_TO_TICKS(30));
        std::string tx = mock->GetTxData();
        EXPECT_NE(tx.find("AT+QIRD=0\r\n"), std::string::npos);
      },
      "URC_QIURC_RECV", 8192, 2, 3000);
  EXPECT_TRUE(ok);
}

FREERTOS_TEST_MAIN()

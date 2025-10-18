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
    if (gsm) {
      gsm->context().end();
      vTaskDelay(pdMS_TO_TICKS(80));
      delete gsm;
    }
    if (mock) delete mock;
    FreeRTOSTest::TearDown();
  }
};

TEST_F(URCRegistrationTest, QIOPEN_URC_SetsConnected) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        bool began = gsm->context().begin(*mock);
        EXPECT_TRUE(began);
        if (!began) return;

        // Inject a successful open URC and verify state transitions to
        // CONNECTED
        InjectRx(mock, "\r\n+QIOPEN: 0,0\r\n");
        vTaskDelay(pdMS_TO_TICKS(20));
        EXPECT_EQ(gsm->context().modem().URCState.isConnected.load(), ConnectionStatus::CONNECTED);
        gsm->context().end();
        vTaskDelay(pdMS_TO_TICKS(80));
      },
      "URC_QIOPEN", 8192, 2, 3000);
  EXPECT_TRUE(ok);
}

TEST_F(URCRegistrationTest, QIURCRecvDeferredUntilAvailable) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        bool began = gsm->context().begin(*mock);
        EXPECT_TRUE(began);
        if (!began) return;

        // Clear any previous TX
        (void)mock->GetTxData();
        // Data-ready URC should not trigger reads until the client polls
        InjectRx(mock, "\r\n+QIURC: \"recv\"\r\n");
        vTaskDelay(pdMS_TO_TICKS(20));
        std::string tx = mock->GetTxData();
        EXPECT_EQ(tx.find("AT+QIRD=0\r\n"), std::string::npos);

        // Calling available() should trigger the read command
        EXPECT_EQ(gsm->available(), 0);
        vTaskDelay(pdMS_TO_TICKS(20));
        tx = mock->GetTxData();
        EXPECT_NE(tx.find("AT+QIRD=0\r\n"), std::string::npos);
        gsm->context().end();
        vTaskDelay(pdMS_TO_TICKS(80));
      },
      "URC_QIURC_RECV", 8192, 2, 3000);
  EXPECT_TRUE(ok);
}

FREERTOS_TEST_MAIN()

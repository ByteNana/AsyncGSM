#include <AsyncGSM.h>
#include <modules/EG915/EG915.h>

#include <atomic>
#include <string>

#include "common/common.h"
#include "common/responder.h"

using ::testing::NiceMock;

class ModemGettersTest : public FreeRTOSTest {
 protected:
  GSMContext *ctx{nullptr};
  NiceMock<MockStream> *mock{nullptr};
  AsyncEG915U module;

  void SetUp() override {
    FreeRTOSTest::SetUp();
    ctx = new GSMContext();
    mock = new NiceMock<MockStream>();
    mock->SetupDefaults();
  }
  void TearDown() override {
    if (ctx) {
      ctx->end();
      vTaskDelay(pdMS_TO_TICKS(50));
      delete ctx;
    }
    if (mock) delete mock;
    FreeRTOSTest::TearDown();
  }
};

TEST_F(ModemGettersTest, GetSimCCID) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(ctx->begin(*mock, module));
        // Provide +QCCID for AT+CCID query
        (void)mock->GetTxData();
        scheduleInject(mock, 10, "\r\n+QCCID: 01234567890123456789\r\nOK\r\n");

        String ccid = ctx->modem().modemInfo().getICCID();
        EXPECT_EQ(ccid.substring(0, 20), String("01234567890123456789"));
        std::string tx = mock->GetTxData();
        EXPECT_NE(tx.find("AT+CCID\r\n"), std::string::npos);
      },
      "GetSimCCID", 8192, 3, 3000);
  EXPECT_TRUE(ok);
}

TEST_F(ModemGettersTest, GetIMEI) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(ctx->begin(*mock, module));
        (void)mock->GetTxData();
        scheduleInject(mock, 10, "\r\n123456789012345\r\nOK\r\n");
        String imei = ctx->modem().modemInfo().getIMEI();
        EXPECT_EQ(imei.substring(0, 15), String("123456789012345"));
        std::string tx = mock->GetTxData();
        EXPECT_NE(tx.find("AT+CGSN\r\n"), std::string::npos);
      },
      "GetIMEI", 8192, 3, 3000);
  EXPECT_TRUE(ok);
}

TEST_F(ModemGettersTest, GetIMSI) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(ctx->begin(*mock, module));
        (void)mock->GetTxData();
        scheduleInject(mock, 10, "\r\n310150123456789\r\nOK\r\n");
        String imsi = ctx->modem().modemInfo().getIMSI();
        EXPECT_EQ(imsi.substring(0, 15), String("310150123456789"));
        std::string tx = mock->GetTxData();
        EXPECT_NE(tx.find("AT+CIMI\r\n"), std::string::npos);
      },
      "GetIMSI", 8192, 3, 3000);
  EXPECT_TRUE(ok);
}

TEST_F(ModemGettersTest, GetOperator) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(ctx->begin(*mock, module));
        (void)mock->GetTxData();
        scheduleInject(mock, 10, "\r\n+COPS: 0,0,\"MockTel\"\r\nOK\r\n");
        String oper = ctx->modem().network().getOperator();
        EXPECT_EQ(oper, String("MockTel"));
        std::string tx = mock->GetTxData();
        EXPECT_NE(tx.find("AT+COPS?\r\n"), std::string::npos);
      },
      "GetOperator", 8192, 3, 3000);
  EXPECT_TRUE(ok);
}

TEST_F(ModemGettersTest, GetIPAddress) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(ctx->begin(*mock, module));
        (void)mock->GetTxData();
        scheduleInject(mock, 10, "\r\n+QIACT: \"10.0.0.2\"\r\nOK\r\n");
        String ip = ctx->modem().modemInfo().getIPAddress();
        EXPECT_EQ(ip, String("10.0.0.2"));
        std::string tx = mock->GetTxData();
        EXPECT_NE(tx.find("AT+QIACT?\r\n"), std::string::npos);
      },
      "GetIP", 8192, 3, 3000);
  EXPECT_TRUE(ok);
}

FREERTOS_TEST_MAIN()
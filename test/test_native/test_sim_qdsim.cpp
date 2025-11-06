#include <AsyncGSM.h>

#include <atomic>
#include <string>

#include "common/common.h"
#include "common/responder.h"

using ::testing::NiceMock;

class SimQDSIMTest : public FreeRTOSTest {
 protected:
  GSMContext *ctx{nullptr};
  NiceMock<MockStream> *mock{nullptr};

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

TEST_F(SimQDSIMTest, GetCurrentSlotParsesResponse) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(ctx->begin(*mock));
        (void)mock->GetTxData();
        // Provide +QDSIM: 1
        scheduleInject(mock, 10, "\r\n+QDSIM: 1\r\nOK\r\n");

        EG915SimSlot slot = ctx->modem().sim.getCurrentSlot();
        EXPECT_EQ(slot, EG915SimSlot::SLOT_2);

        std::string tx = mock->GetTxData();
        EXPECT_NE(tx.find("AT+QDSIM?\r\n"), std::string::npos);
      },
      "GetCurrentSlotParsesResponse", 8192, 3, 3000);
  EXPECT_TRUE(ok);
}

TEST_F(SimQDSIMTest, SetSlotSendsCommand) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(ctx->begin(*mock));
        (void)mock->GetTxData();
        // Generic OK responder in mocks will acknowledge the command
        scheduleInject(mock, 10, "\r\nOK\r\n");

        bool res = ctx->modem().sim.setSlot(EG915SimSlot::SLOT_2);
        EXPECT_TRUE(res);

        std::string tx = mock->GetTxData();
        EXPECT_NE(tx.find("AT+QDSIM=1\r\n"), std::string::npos);
      },
      "SetSlotSendsCommand", 8192, 3, 3000);
  EXPECT_TRUE(ok);
}

TEST_F(SimQDSIMTest, GetCurrentSlotSlot1) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(ctx->begin(*mock));
        (void)mock->GetTxData();
        scheduleInject(mock, 10, "\r\n+QDSIM: 0\r\nOK\r\n");

        EG915SimSlot slot = ctx->modem().sim.getCurrentSlot();
        EXPECT_EQ(slot, EG915SimSlot::SLOT_1);

        std::string tx = mock->GetTxData();
        EXPECT_NE(tx.find("AT+QDSIM?\r\n"), std::string::npos);
      },
      "GetCurrentSlotSlot1", 8192, 3, 3000);
  EXPECT_TRUE(ok);
}

TEST_F(SimQDSIMTest, GetCurrentSlotInvalidResponseKeepsUnknown) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(ctx->begin(*mock));
        (void)mock->GetTxData();
        // Invalid value 3 should fail parse and keep UNKNOWN
        scheduleInject(mock, 10, "\r\n+QDSIM: 3\r\nOK\r\n");
        EG915SimSlot slot = ctx->modem().sim.getCurrentSlot();
        EXPECT_EQ(slot, EG915SimSlot::UNKNOWN);
        std::string tx = mock->GetTxData();
        EXPECT_NE(tx.find("AT+QDSIM?\r\n"), std::string::npos);
      },
      "GetCurrentSlotInvalidResponseKeepsUnknown", 8192, 3, 3000);
  EXPECT_TRUE(ok);
}

TEST_F(SimQDSIMTest, GetCurrentSlotInvalidResponsePreservesCached) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(ctx->begin(*mock));
        (void)mock->GetTxData();

        // First, set slot to SLOT_1 (updates cache)
        scheduleInject(mock, 10, "\r\nOK\r\n");
        EXPECT_TRUE(ctx->modem().sim.setSlot(EG915SimSlot::SLOT_1));
        (void)mock->GetTxData();

        // Now return invalid response to getCurrentSlot
        scheduleInject(mock, 20, "\r\n+QDSIM: 9\r\nOK\r\n");
        EG915SimSlot slot = ctx->modem().sim.getCurrentSlot();
        EXPECT_EQ(slot, EG915SimSlot::SLOT_1);

        std::string tx = mock->GetTxData();
        EXPECT_NE(tx.find("AT+QDSIM?\r\n"), std::string::npos);
      },
      "GetCurrentSlotInvalidResponsePreservesCached", 8192, 3, 3000);
  EXPECT_TRUE(ok);
}

TEST_F(SimQDSIMTest, SetSlotRejectsUnknown) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(ctx->begin(*mock));
        (void)mock->GetTxData();
        bool res = ctx->modem().sim.setSlot(EG915SimSlot::UNKNOWN);
        EXPECT_FALSE(res);
        std::string tx = mock->GetTxData();
        EXPECT_EQ(tx.find("AT+QDSIM="), std::string::npos);
      },
      "SetSlotRejectsUnknown", 8192, 3, 3000);
  EXPECT_TRUE(ok);
}

TEST_F(SimQDSIMTest, SetSlotNoOpWhenSame) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(ctx->begin(*mock));
        (void)mock->GetTxData();

        // Set to SLOT_2 first
        scheduleInject(mock, 10, "\r\nOK\r\n");
        EXPECT_TRUE(ctx->modem().sim.setSlot(EG915SimSlot::SLOT_2));
        (void)mock->GetTxData();

        // Call again with same slot; should not send AT+QDSIM
        bool res = ctx->modem().sim.setSlot(EG915SimSlot::SLOT_2);
        EXPECT_TRUE(res);
        std::string tx = mock->GetTxData();
        EXPECT_EQ(tx.find("AT+QDSIM="), std::string::npos);
      },
      "SetSlotNoOpWhenSame", 8192, 3, 3000);
  EXPECT_TRUE(ok);
}

// === QSIMDET and QSIMSTAT tests merged here ===

TEST_F(SimQDSIMTest, QSIMDETParsesBothSlots) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(ctx->begin(*mock));
        (void)mock->GetTxData();
        // +QSIMDET: (1,1),(0,0)
        scheduleInject(mock, 10, "\r\n+QSIMDET: (1,1),(0,0)\r\nOK\r\n");

        EG915SimDetDual det = ctx->modem().sim.getDetection();
        EXPECT_EQ(det.sim1.cardDetection, EG915SimDetConfig::CardDetection::ENABLE);
        EXPECT_EQ(det.sim1.insertLevel, EG915SimDetConfig::InsertLevel::HIGH_LEVEL);
        EXPECT_EQ(det.sim2.cardDetection, EG915SimDetConfig::CardDetection::DISABLE);
        EXPECT_EQ(det.sim2.insertLevel, EG915SimDetConfig::InsertLevel::LOW_LEVEL);

        std::string tx = mock->GetTxData();
        EXPECT_NE(tx.find("AT+QSIMDET?\r\n"), std::string::npos);
      },
      "QSIMDETParsesBothSlots", 8192, 3, 3000);
  EXPECT_TRUE(ok);
}

TEST_F(SimQDSIMTest, QSIMSTATParsesInserted) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(ctx->begin(*mock));
        (void)mock->GetTxData();
        scheduleInject(mock, 10, "\r\n+QSIMSTAT: 1,1\r\nOK\r\n");

        EG915SimStatus st = ctx->modem().sim.getStatus();
        EXPECT_EQ(st.enable, EG915SimStatus::ReportState::ENABLE);
        EXPECT_EQ(st.inserted, EG915SimStatus::InsertStatus::INSERTED);

        std::string tx = mock->GetTxData();
        EXPECT_NE(tx.find("AT+QSIMSTAT?\r\n"), std::string::npos);
      },
      "QSIMSTATParsesInserted", 8192, 3, 3000);
  EXPECT_TRUE(ok);
}

TEST_F(SimQDSIMTest, QSIMSTATParsesUnknown) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(ctx->begin(*mock));
        (void)mock->GetTxData();
        scheduleInject(mock, 10, "\r\n+QSIMSTAT: 1,2\r\nOK\r\n");

        EG915SimStatus st = ctx->modem().sim.getStatus();
        EXPECT_EQ(st.enable, EG915SimStatus::ReportState::ENABLE);
        EXPECT_EQ(st.inserted, EG915SimStatus::InsertStatus::UNKNOWN);

        std::string tx = mock->GetTxData();
        EXPECT_NE(tx.find("AT+QSIMSTAT?\r\n"), std::string::npos);
      },
      "QSIMSTATParsesUnknown", 8192, 3, 3000);
  EXPECT_TRUE(ok);
}

TEST_F(SimQDSIMTest, SetStatusReportSendsEnableDisable) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(ctx->begin(*mock));
        (void)mock->GetTxData();

        // Enable
        scheduleInject(mock, 10, "\r\nOK\r\n");
        EXPECT_TRUE(ctx->modem().sim.setStatusReport(true));
        std::string tx1 = mock->GetTxData();
        EXPECT_NE(tx1.find("AT+QSIMSTAT=1\r\n"), std::string::npos);

        // Disable
        scheduleInject(mock, 20, "\r\nOK\r\n");
        EXPECT_TRUE(ctx->modem().sim.setStatusReport(false));
        std::string tx2 = mock->GetTxData();
        EXPECT_NE(tx2.find("AT+QSIMSTAT=0\r\n"), std::string::npos);
      },
      "SetStatusReportSendsEnableDisable", 8192, 3, 3000);
  EXPECT_TRUE(ok);
}

TEST_F(SimQDSIMTest, QSIMSTATInvalidResponseDefaults) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(ctx->begin(*mock));
        (void)mock->GetTxData();
        scheduleInject(mock, 10, "\r\n+QSIMSTAT: 9,9\r\nOK\r\n");
        EG915SimStatus st = ctx->modem().sim.getStatus();
        // Defaults when parse fails
        EXPECT_EQ(st.enable, EG915SimStatus::ReportState::DISABLE);
        EXPECT_EQ(st.inserted, EG915SimStatus::InsertStatus::UNKNOWN);
        std::string tx = mock->GetTxData();
        EXPECT_NE(tx.find("AT+QSIMSTAT?\r\n"), std::string::npos);
      },
      "QSIMSTATInvalidResponseDefaults", 8192, 3, 3000);
  EXPECT_TRUE(ok);
}

FREERTOS_TEST_MAIN()

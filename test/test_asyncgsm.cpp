#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "AsyncGSM.h"
#include "Stream.h" // from AsyncATHandler mocks

using ::testing::InSequence;
using ::testing::Invoke;

TEST(AsyncGSMTest, GprsConnectSendsCommands) {
  MockStream stream;
  stream.SetupDefaults();

  AsyncGSM gsm;
  ASSERT_TRUE(gsm.init(stream));

  {
    InSequence seq;
    EXPECT_CALL(stream, write(::testing::_, ::testing::_))
        .WillOnce(Invoke([&](const uint8_t *buf, size_t size) {
          std::string cmd(reinterpret_cast<const char *>(buf), size);
          EXPECT_EQ(cmd, "AT+QIDEACT=1");
          stream.InjectRxData("OK\r\n");
          return size;
        }));

    EXPECT_CALL(stream, write(::testing::_, ::testing::_))
        .WillOnce(Invoke([&](const uint8_t *buf, size_t size) {
          std::string cmd(reinterpret_cast<const char *>(buf), size);
          EXPECT_EQ(cmd, "AT+QICSGP=1,1,\"apn\",\"user\", \"pwd");
          stream.InjectRxData("OK\r\n");
          return size;
        }));

    EXPECT_CALL(stream, write(::testing::_, ::testing::_))
        .WillOnce(Invoke([&](const uint8_t *buf, size_t size) {
          std::string cmd(reinterpret_cast<const char *>(buf), size);
          EXPECT_EQ(cmd, "AT+QIACT=1");
          stream.InjectRxData("OK\r\n");
          return size;
        }));

    EXPECT_CALL(stream, write(::testing::_, ::testing::_))
        .WillOnce(Invoke([&](const uint8_t *buf, size_t size) {
          std::string cmd(reinterpret_cast<const char *>(buf), size);
          EXPECT_EQ(cmd, "AT+CGATT=1");
          stream.InjectRxData("OK\r\n");
          return size;
        }));
  }

  EXPECT_TRUE(gsm.gprsConnect("apn", "user", "pwd"));
}

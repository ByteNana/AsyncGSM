#include <Arduino.h>
#include <unity.h>
#include "AsyncGSM.h"
#include "mocks.h"

AsyncGSM modem;
HardwareMockStream mockSerial;

void setUp(void) {
  mockSerial.clearSentData();
}

void tearDown(void) {}

void test_get_sim_ccid() {
  mockSerial.mockResponse("AT+QCCID\r\n+QCCID: 01234567890123456789\r\nOK\r\n");
  String ccid = modem.getSimCCID();
  TEST_ASSERT_EQUAL_STRING("AT+QCCID\r\n", mockSerial.getSentData().c_str());
  TEST_ASSERT_EQUAL_STRING("01234567890123456789", ccid.c_str());
}

void test_gprs_disconnect() {
  mockSerial.mockResponse("AT+QIDEACT=1\r\nOK\r\n");
  bool ok = modem.gprsDisconnect();
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_STRING("AT+QIDEACT=1\r\n", mockSerial.getSentData().c_str());
}

void test_gprs_connect() {
  mockSerial.mockResponse(
    "AT+QIDEACT=1\r\nOK\r\n"
    "AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"\r\nOK\r\n"
    "AT+QIACT=1\r\nOK\r\n"
    "AT+CGATT=1\r\nOK\r\n");
  bool ok = modem.gprsConnect("internet", "user", "pass");
  String expected =
    "AT+QIDEACT=1\r\n"
    "AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"\r\n"
    "AT+QIACT=1\r\n"
    "AT+CGATT=1\r\n";
  TEST_ASSERT_EQUAL_STRING(expected.c_str(), mockSerial.getSentData().c_str());
  TEST_ASSERT_TRUE(ok);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  UNITY_BEGIN();
  TEST_ASSERT_TRUE(modem.init(mockSerial));
  RUN_TEST(test_get_sim_ccid);
  RUN_TEST(test_gprs_disconnect);
  RUN_TEST(test_gprs_connect);
  UNITY_END();
}

void loop() {
}


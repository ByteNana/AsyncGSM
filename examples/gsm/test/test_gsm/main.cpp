#include <Arduino.h>
#include <AsyncGSM.h>
#include <unity.h>

#include "mocks.h"

AsyncGSM gsm;
HardwareMockStream mockSerial;

void setUp(void) { mockSerial.clearSentData(); }

void tearDown(void) {}

void test_get_sim_ccid() {
  mockSerial.mockResponse("AT+QCCID\r\n+QCCID: 01234567890123456789\r\nOK\r\n");
  String ccid = gsm.context().modem().getSimCCID();
  TEST_ASSERT_EQUAL_STRING("AT+QCCID\r\n", mockSerial.getSentData().c_str());
  TEST_ASSERT_EQUAL_STRING("01234567890123456789", ccid.c_str());
}

void test_get_imei() {
  mockSerial.mockResponse("AT+CGSN\r\n123456789012345\r\nOK\r\n");
  String imei = gsm.context().modem().getIMEI();
  TEST_ASSERT_EQUAL_STRING("AT+CGSN\r\n", mockSerial.getSentData().c_str());
  TEST_ASSERT_EQUAL_STRING("123456789012345", imei.c_str());
}

void test_get_imsi() {
  mockSerial.mockResponse("AT+CIMI\r\n310150123456789\r\nOK\r\n");
  String imsi = gsm.context().modem().getIMSI();
  TEST_ASSERT_EQUAL_STRING("AT+CIMI\r\n", mockSerial.getSentData().c_str());
  TEST_ASSERT_EQUAL_STRING("310150123456789", imsi.c_str());
}

void test_get_operator() {
  mockSerial.mockResponse("AT+COPS?\r\n+COPS: 0,0,\"MockTel\"\r\nOK\r\n");
  String oper = gsm.context().modem().getOperator();
  TEST_ASSERT_EQUAL_STRING("AT+COPS?\r\n", mockSerial.getSentData().c_str());
  TEST_ASSERT_EQUAL_STRING("MockTel", oper.c_str());
}

void test_get_ip() {
  mockSerial.mockResponse("AT+CGPADDR=1\r\n+CGPADDR: 1,10.0.0.2\r\nOK\r\n");
  String ip = gsm.context().modem().getIPAddress();
  TEST_ASSERT_EQUAL_STRING("AT+CGPADDR=1\r\n", mockSerial.getSentData().c_str());
  TEST_ASSERT_EQUAL_STRING("10.0.0.2", ip.c_str());
}

void test_gprs_disconnect() {
  mockSerial.mockResponse("AT+QIDEACT=1\r\nOK\r\n");
  bool ok = gsm.gprsDisconnect();
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_STRING("AT+QIDEACT=1\r\n", mockSerial.getSentData().c_str());
}

void test_gprs_connect() {
  mockSerial.mockResponse(
      "AT+QIDEACT=1\r\nOK\r\n"
      "AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"\r\nOK\r\n"
      "AT+QIACT=1\r\nOK\r\n"
      "AT+CGATT=1\r\nOK\r\n");
  bool ok = gsm.context().modem().gprsConnect("internet", "user", "pass");
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
  TEST_ASSERT_TRUE(gsm.context().begin(mockSerial));
  TEST_ASSERT_TRUE(gsm.context().setupNetwork("test_apn"));
  RUN_TEST(test_get_sim_ccid);
  RUN_TEST(test_gprs_disconnect);
  RUN_TEST(test_gprs_connect);
  UNITY_END();
}

void loop() {}

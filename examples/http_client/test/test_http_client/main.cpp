#include <Arduino.h>
#include <AsyncGSM.h>
#include <HttpClient.h>
#include <unity.h>

#include "mocks.h"

AsyncGSM modem;
HardwareMockStream mockSerial;

const char server[] = "example.com";
const int port = 80;
HttpClient http(modem, server, port);

void setUp(void) { mockSerial.clearSentData(); }
void tearDown(void) {}

void test_http_get() {
  mockSerial.mockResponse(
      "AT+QIDEACT=1\r\nOK\r\n"
      "AT+QIOPEN=1,0,\"TCP\",\"example.com\",80,0,0\r\n+QIOPEN: 0,0\r\nOK\r\n"
      "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHello");

  int ret = http.get("/");
  TEST_ASSERT_EQUAL(HTTP_SUCCESS, ret);

  String expected =
      "AT+QIDEACT=1\r\n"
      "AT+QIOPEN=1,0,\"TCP\",\"example.com\",80,0,0\r\n"
      "GET / HTTP/1.1\r\n"
      "Host: example.com\r\n"
      "User-Agent: Arduino/2.2.0\r\n"
      "Connection: close\r\n"
      "\r\n";
  TEST_ASSERT_EQUAL_STRING(expected.c_str(), mockSerial.getSentData().c_str());

  int status = http.responseStatusCode();
  TEST_ASSERT_EQUAL(200, status);
  String body = http.responseBody();
  TEST_ASSERT_EQUAL_STRING("Hello", body.c_str());
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  UNITY_BEGIN();
  TEST_ASSERT_TRUE(modem.context().begin(mockSerial));
  TEST_ASSERT_TRUE(modem.context().setupNetwork("test_apn"));
  RUN_TEST(test_http_get);
  UNITY_END();
}

void loop() {}

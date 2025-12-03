// Test helpers to simulate TCP socket behavior over AT commands and capture
// payload
#pragma once

#include <gmock/gmock.h>

#include <atomic>
#include <cstdio>
#include <string>

#include "common.h"

using ::testing::NiceMock;

#if LOG_LEVEL == 5
#define SERIAL_LOG(dir, strref)             \
  do {                                      \
    const std::string &_s = (strref);       \
    printf("[%s] %s\n", (dir), _s.c_str()); \
    fflush(stdout);                         \
  } while (0)
#else
#define SERIAL_LOG(dir, strref) \
  do { (void)sizeof(strref); } while (0)
#endif

inline void InjectRx(class MockStream *s, const std::string &data) {
  SERIAL_LOG("RX >", data);
  s->InjectRxData(data);
}

inline void InjectRx(class MockStream *s, const char *data) { InjectRx(s, std::string(data)); }

inline std::string DrainTx(class MockStream *s) {
  std::string chunk = s->GetTxData();
  if (!chunk.empty()) { SERIAL_LOG("TX <", chunk); }
  return chunk;
}

// Basic responder: handles QIOPEN/QISEND/QICLOSE and injects modem responses
inline void startTcpResponder(
    NiceMock<MockStream> *s, std::atomic<bool> *done, bool failOpen = false) {
  auto responder = [](void *pv) {
    using Ctx = std::tuple<NiceMock<MockStream> *, std::atomic<bool> *, bool, std::atomic<bool> *>;
    auto *ctx = static_cast<Ctx *>(pv);
    auto *s = std::get<0>(*ctx);
    auto *done = std::get<1>(*ctx);
    bool failOpen = std::get<2>(*ctx);
    auto *started = std::get<3>(*ctx);

    std::string acc;
    TickType_t start = xTaskGetTickCount();
    const TickType_t maxTicks = pdMS_TO_TICKS(10000);
    if (started) started->store(true);
    while (!done->load()) {
      if ((xTaskGetTickCount() - start) > maxTicks) break;  // safety bound

      std::string chunk = DrainTx(s);
      if (!chunk.empty()) acc += chunk;

      size_t pos;
      while ((pos = acc.find("\r\n")) != std::string::npos) {
        std::string cmd = acc.substr(0, pos);
        acc.erase(0, pos + 2);
        auto starts_with = [&](const char *p) { return cmd.rfind(p, 0) == 0; };

        if (starts_with("AT+QIOPEN")) {
          InjectRx(s, "OK\r\n");
          InjectRx(s, failOpen ? "+QIOPEN: 0,1\r\n" : "+QIOPEN: 0,0\r\n");
          continue;
        }
        if (starts_with("AT+QISEND=")) {
          InjectRx(s, ">\r\n");
          vTaskDelay(pdMS_TO_TICKS(1));
          InjectRx(s, "OK\r\n");
          InjectRx(s, "SEND OK\r\n");
          continue;
        }
        if (starts_with("AT+QICLOSE")) {
          // Real modems typically reply OK, then emit a close URC shortly after
          InjectRx(s, "OK\r\n");
          vTaskDelay(pdMS_TO_TICKS(1));
          InjectRx(s, "+QIURC: \"closed\"\r\n");
          continue;
        }
        // Generic OK for any other AT+ commands so features (e.g., MQTT cfg) don't time out
        if (starts_with("AT+")) {
          InjectRx(s, "OK\r\n");
          continue;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    delete started;
    delete ctx;
    vTaskDelete(nullptr);
  };
  auto *started = new std::atomic<bool>(false);
  auto *ctx =
      new std::tuple<NiceMock<MockStream> *, std::atomic<bool> *, bool, std::atomic<bool> *>(
          s, done, failOpen, started);
  // Use slightly higher priority so responder starts before first AT command
  xTaskCreate(responder, "TCP_RESP", configMINIMAL_STACK_SIZE * 4, ctx, 3, nullptr);
  // Wait briefly for responder to report it has started
  TickType_t t0 = xTaskGetTickCount();
  while (!started->load() && (xTaskGetTickCount() - t0) < pdMS_TO_TICKS(50)) {
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// Capturing responder: as above, but captures payload lines (non-AT) into
// outCapture
inline void startTcpResponderCapturing(
    NiceMock<MockStream> *s, std::atomic<bool> *done, std::string *outCapture,
    bool failOpen = false) {
  auto responder = [](void *pv) {
    using Ctx = std::tuple<
        NiceMock<MockStream> *, std::atomic<bool> *, std::string *, bool, std::atomic<bool> *>;
    auto *ctx = static_cast<Ctx *>(pv);
    auto *s = std::get<0>(*ctx);
    auto *done = std::get<1>(*ctx);
    auto *cap = std::get<2>(*ctx);
    bool failOpen = std::get<3>(*ctx);
    auto *started = std::get<4>(*ctx);

    std::string acc;
    TickType_t start = xTaskGetTickCount();
    const TickType_t maxTicks = pdMS_TO_TICKS(10000);
    if (started) started->store(true);
    while (!done->load()) {
      if ((xTaskGetTickCount() - start) > maxTicks) break;  // safety bound

      std::string chunk = DrainTx(s);
      if (!chunk.empty()) acc += chunk;

      size_t pos;
      while ((pos = acc.find("\r\n")) != std::string::npos) {
        std::string cmd = acc.substr(0, pos);
        acc.erase(0, pos + 2);
        auto starts_with = [&](const char *p) { return cmd.rfind(p, 0) == 0; };

        if (starts_with("AT+QIOPEN")) {
          InjectRx(s, "OK\r\n");
          InjectRx(s, failOpen ? "+QIOPEN: 0,1\r\n" : "+QIOPEN: 0,0\r\n");
          continue;
        }
        if (starts_with("AT+QISEND=")) {
          InjectRx(s, ">\r\n");
          vTaskDelay(pdMS_TO_TICKS(1));
          InjectRx(s, "OK\r\n");
          InjectRx(s, "SEND OK\r\n");
          continue;
        }
        if (starts_with("AT+QICLOSE")) {
          InjectRx(s, "OK\r\n");
          vTaskDelay(pdMS_TO_TICKS(1));
          InjectRx(s, "+QIURC: \"closed\"\r\n");
          continue;
        }

        // Generic OK for other AT+ commands to avoid swallowing them
        if (starts_with("AT+")) {
          InjectRx(s, "OK\r\n");
          continue;
        }
        // Capture payload lines
        if (cap) {
          (*cap) += cmd;
          (*cap) += "\r\n";
        }
      }
      // Capture any trailing bytes not terminated by CRLF (e.g., HTTP body)
      if (!acc.empty()) {
        if (acc.rfind("AT+", 0) != 0 && cap) {
          (*cap) += acc;
          acc.clear();
        }
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    delete started;
    delete ctx;
    vTaskDelete(nullptr);
  };
  auto *started = new std::atomic<bool>(false);
  auto *ctx = new std::tuple<
      NiceMock<MockStream> *, std::atomic<bool> *, std::string *, bool, std::atomic<bool> *>(
      s, done, outCapture, failOpen, started);
  xTaskCreate(responder, "TCP_RESP_CAP", configMINIMAL_STACK_SIZE * 4, ctx, 3, nullptr);
  TickType_t t0 = xTaskGetTickCount();
  while (!started->load() && (xTaskGetTickCount() - t0) < pdMS_TO_TICKS(50)) {
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

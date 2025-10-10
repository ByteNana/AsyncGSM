// Test helpers to simulate TCP socket behavior over AT commands and capture payload
#pragma once

#include "common.h"
#include <gmock/gmock.h>
#include <atomic>
#include <string>
#include <cstdio>

using ::testing::NiceMock;

#if LOG_LEVEL == 5
#define SERIAL_LOG(dir, strref)                                                \
  do {                                                                         \
    const std::string& _s = (strref);                                          \
    printf("[SERIAL %s] %s\n", (dir), _s.c_str());                            \
    fflush(stdout);                                                            \
  } while (0)
#else
#define SERIAL_LOG(dir, strref) do { (void)sizeof(strref); } while (0)
#endif

// Basic responder: handles QIOPEN/QISEND/QICLOSE and injects modem responses
inline void startTcpResponder(NiceMock<MockStream>* s, std::atomic<bool>* done,
                              bool failOpen = false) {
  auto responder = [](void* pv) {
    auto* ctx = static_cast<std::tuple<NiceMock<MockStream>*, std::atomic<bool>*, bool>*>(pv);
    auto* s = std::get<0>(*ctx);
    auto* done = std::get<1>(*ctx);
    bool failOpen = std::get<2>(*ctx);

    std::string acc;
    TickType_t start = xTaskGetTickCount();
    const TickType_t maxTicks = pdMS_TO_TICKS(10000);
    while (!done->load()) {
      if ((xTaskGetTickCount() - start) > maxTicks) break; // safety bound

      std::string chunk = s->GetTxData();
      if (!chunk.empty()) acc += chunk;

      size_t pos;
      while ((pos = acc.find("\r\n")) != std::string::npos) {
        std::string cmd = acc.substr(0, pos);
        acc.erase(0, pos + 2);
        auto starts_with = [&](const char* p) { return cmd.rfind(p, 0) == 0; };

        if (starts_with("AT+QIOPEN")) {
          SERIAL_LOG("TX", cmd);
          s->InjectRxData("OK\r\n");
          SERIAL_LOG("RX", std::string("OK"));
          s->InjectRxData(failOpen ? "+QIOPEN: 0,1\r\n" : "+QIOPEN: 0,0\r\n");
          SERIAL_LOG("RX", failOpen ? std::string("+QIOPEN: 0,1") : std::string("+QIOPEN: 0,0"));
          continue;
        }
        if (starts_with("AT+QISEND=")) {
          SERIAL_LOG("TX", cmd);
          s->InjectRxData(">\r\n");
          SERIAL_LOG("RX", std::string(">"));
          vTaskDelay(pdMS_TO_TICKS(1));
          s->InjectRxData("OK\r\n");
          SERIAL_LOG("RX", std::string("OK"));
          s->InjectRxData("SEND OK\r\n");
          SERIAL_LOG("RX", std::string("SEND OK"));
          continue;
        }
        if (starts_with("AT+QICLOSE")) {
          SERIAL_LOG("TX", cmd);
          s->InjectRxData("OK\r\n");
          SERIAL_LOG("RX", std::string("OK"));
          continue;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    delete ctx;
    vTaskDelete(nullptr);
  };
  auto* ctx = new std::tuple<NiceMock<MockStream>*, std::atomic<bool>*, bool>(s, done, failOpen);
  xTaskCreate(responder, "TCP_RESP", configMINIMAL_STACK_SIZE * 4, ctx, 1, nullptr);
}

// Capturing responder: as above, but captures payload lines (non-AT) into outCapture
inline void startTcpResponderCapturing(NiceMock<MockStream>* s, std::atomic<bool>* done,
                                       std::string* outCapture, bool failOpen = false) {
  auto responder = [](void* pv) {
    auto* ctx = static_cast<std::tuple<NiceMock<MockStream>*, std::atomic<bool>*, std::string*, bool>*>(pv);
    auto* s = std::get<0>(*ctx);
    auto* done = std::get<1>(*ctx);
    auto* cap = std::get<2>(*ctx);
    bool failOpen = std::get<3>(*ctx);

    std::string acc;
    TickType_t start = xTaskGetTickCount();
    const TickType_t maxTicks = pdMS_TO_TICKS(10000);
    while (!done->load()) {
      if ((xTaskGetTickCount() - start) > maxTicks) break; // safety bound

      std::string chunk = s->GetTxData();
      if (!chunk.empty()) acc += chunk;

      size_t pos;
      while ((pos = acc.find("\r\n")) != std::string::npos) {
        std::string cmd = acc.substr(0, pos);
        acc.erase(0, pos + 2);
        auto starts_with = [&](const char* p) { return cmd.rfind(p, 0) == 0; };

        if (starts_with("AT+QIOPEN")) {
          SERIAL_LOG("TX", cmd);
          s->InjectRxData("OK\r\n");
          SERIAL_LOG("RX", std::string("OK"));
          s->InjectRxData(failOpen ? "+QIOPEN: 0,1\r\n" : "+QIOPEN: 0,0\r\n");
          SERIAL_LOG("RX", failOpen ? std::string("+QIOPEN: 0,1") : std::string("+QIOPEN: 0,0"));
          continue;
        }
        if (starts_with("AT+QISEND=")) {
          SERIAL_LOG("TX", cmd);
          s->InjectRxData(">\r\n");
          SERIAL_LOG("RX", std::string(">"));
          vTaskDelay(pdMS_TO_TICKS(1));
          s->InjectRxData("OK\r\n");
          SERIAL_LOG("RX", std::string("OK"));
          s->InjectRxData("SEND OK\r\n");
          SERIAL_LOG("RX", std::string("SEND OK"));
          continue;
        }
        if (starts_with("AT+QICLOSE")) {
          SERIAL_LOG("TX", cmd);
          s->InjectRxData("OK\r\n");
          SERIAL_LOG("RX", std::string("OK"));
          continue;
        }
        // Capture payload lines
        if (cap) {
          SERIAL_LOG("TX", cmd);
          (*cap) += cmd;
          (*cap) += "\r\n";
        }
      }
      // Capture any trailing bytes not terminated by CRLF (e.g., HTTP body)
      if (!acc.empty()) {
        if (acc.rfind("AT+", 0) != 0 && cap) {
          SERIAL_LOG("TX", acc);
          (*cap) += acc;
          acc.clear();
        }
      }
      vTaskDelay(pdMS_TO_TICKS(1));
    }
    delete ctx;
    vTaskDelete(nullptr);
  };
  auto* ctx = new std::tuple<NiceMock<MockStream>*, std::atomic<bool>*, std::string*, bool>(s, done, outCapture, failOpen);
  xTaskCreate(responder, "TCP_RESP_CAP", configMINIMAL_STACK_SIZE * 4, ctx, 1, nullptr);
}

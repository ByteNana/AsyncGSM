#pragma once

#include "FreeRTOSConfig.h"
#include "Stream.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <mutex>
#include <condition_variable>

class GlobalSchedulerEnvironment : public ::testing::Environment {
private:
  static std::thread globalSchedulerThread;
  static std::atomic<bool> globalSchedulerStarted;
  static std::atomic<bool> globalSchedulerRunning;
  static std::mutex schedMutex;
  static std::condition_variable schedCv;

public:
  void SetUp() override {
    // Start only once per process
    if (!globalSchedulerRunning.load()) {
      globalSchedulerThread = std::thread([]() {
        {
          std::lock_guard<std::mutex> lk(schedMutex);
          globalSchedulerStarted = true;
          globalSchedulerRunning = true;
        }
        schedCv.notify_all();

        vTaskStartScheduler();

        // When the scheduler stops, mark running=false and notify waiters.
        {
          std::lock_guard<std::mutex> lk(schedMutex);
          globalSchedulerRunning = false;
        }
        schedCv.notify_all();
      });
    }
    // Wait (bounded) until the scheduler thread reports running=true
    {
      std::unique_lock<std::mutex> lk(schedMutex);
      schedCv.wait_for(lk, std::chrono::milliseconds(2000), [] {
        return globalSchedulerRunning.load();
      });
    }
    // Small grace period to let timers/idle task spin up
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  void TearDown() override {
    if (globalSchedulerThread.joinable()) {
      vTaskEndScheduler();
      // Wait (bounded) for the scheduler thread to acknowledge shutdown
      {
        std::unique_lock<std::mutex> lk(schedMutex);
        schedCv.wait_for(lk, std::chrono::milliseconds(1000), [] {
          return !globalSchedulerRunning.load();
        });
      }
      // Join if it finished in time; otherwise detach to avoid deadlock
      if (globalSchedulerRunning.load()) {
        globalSchedulerThread.detach();
      } else {
        globalSchedulerThread.join();
      }
    }
  }
};

// Define static members
inline std::thread GlobalSchedulerEnvironment::globalSchedulerThread;
inline std::atomic<bool> GlobalSchedulerEnvironment::globalSchedulerStarted{false};
inline std::atomic<bool> GlobalSchedulerEnvironment::globalSchedulerRunning{false};
inline std::mutex GlobalSchedulerEnvironment::schedMutex;
inline std::condition_variable GlobalSchedulerEnvironment::schedCv;

// Helper function to run code in a FreeRTOS task context
inline bool runInFreeRTOSTask(std::function<void()> func,
                              const char *taskName = "HelperTask",
                              uint32_t stackSize = 2048,
                              UBaseType_t priority = 2,
                              uint32_t timeoutMs = 5000) {
  std::atomic<bool> taskComplete{false};
  std::atomic<bool> taskResult{true};

  auto taskWrapper = [](void *pvParameters) {
    struct TaskData {
      std::function<void()> *function;
      std::atomic<bool> *complete;
      std::atomic<bool> *result;
    };
    auto *data = static_cast<TaskData *>(pvParameters);

    try {
      (*data->function)();
    } catch (const std::exception &e) {
      log_e("[FreeRTOS Task] Exception caught: %s", e.what());
      data->result->store(false);
    } catch (...) {
      log_e("[FreeRTOS Task] Unknown exception caught");
      data->result->store(false);
    }

    data->complete->store(true);
    vTaskDelete(nullptr);
  };

  struct TaskData {
    std::function<void()> *function;
    std::atomic<bool> *complete;
    std::atomic<bool> *result;
  } taskData = {&func, &taskComplete, &taskResult};

  TaskHandle_t taskHandle = nullptr;
  BaseType_t result = xTaskCreate(taskWrapper, taskName, stackSize, &taskData,
                                  priority, &taskHandle);

  if (result != pdPASS) {
    return false;
  }

  // Wait for completion with timeout
  uint32_t waitTime = 0;
  while (!taskComplete.load() && waitTime < timeoutMs) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    waitTime += 10;
  }
  // If the task is still running after timeout, force-delete it to avoid hangs
  if (!taskComplete.load() && taskHandle != nullptr) {
    // Best-effort cancellation of the task to prevent sticking around.
    vTaskDelete(taskHandle);
    // Mark as failed; caller will see false and proceed.
    taskResult.store(false);
  }
  return taskComplete.load() && taskResult.load();
}

// Helper macro to set up the environment in main
#define FREERTOS_TEST_MAIN()                                                   \
  int main(int argc, char **argv) {                                            \
    ::testing::InitGoogleTest(&argc, argv);                                    \
    ::testing::AddGlobalTestEnvironment(new GlobalSchedulerEnvironment);       \
    return RUN_ALL_TESTS();                                                    \
  }

// Base test class with common teardown
class FreeRTOSTest : public ::testing::Test {
protected:
  void TearDown() override {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
};

inline void scheduleInject(class MockStream *stream, uint32_t delayMs,
                           std::string payload) {
  struct Ctx {
    MockStream *s;
    std::string p;
    uint32_t d;
  };
  auto th = [](void *pv) {
    std::unique_ptr<Ctx> ctx(static_cast<Ctx *>(pv)); // RAII
    vTaskDelay(pdMS_TO_TICKS(ctx->d));
    ctx->s->InjectRxData(ctx->p);
    vTaskDelete(nullptr);
  };
  auto *ctx = new Ctx{stream, std::move(payload), delayMs};
  printf("Scheduling inject of %zu bytes in %u ms\n", ctx->p.size(), ctx->d);
  if (xTaskCreate(th, "DelayedInject", configMINIMAL_STACK_SIZE + 2000, ctx, 1,
                  nullptr) != pdPASS) {
    printf("Failed to create inject task\n");
    delete ctx;
  }
}

inline void InjectDataWithDelay(class MockStream *mockStream,
                                const std::string &data,
                                uint32_t delayMs = 50) {
  struct InjectorData {
    MockStream *stream;
    std::string data;
    uint32_t delay;
  };

  auto *injectorData = new InjectorData{mockStream, data, delayMs};

  auto injectorTask = [](void *pvParameters) {
    auto *data = static_cast<InjectorData *>(pvParameters);
    vTaskDelay(pdMS_TO_TICKS(data->delay));
    data->stream->InjectRxData(data->data);
    delete data;
    vTaskDelete(nullptr);
  };

  TaskHandle_t injectorHandle = nullptr;
  xTaskCreate(injectorTask, "InjectorTask", configMINIMAL_STACK_SIZE * 2,
              injectorData, 1, &injectorHandle);
}


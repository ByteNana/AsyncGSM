#pragma once
#include <stdint.h>

template <typename E>
constexpr auto u8(E e) {
  return static_cast<uint8_t>(e);
}

// =========================================================
// SIM Detection Configuration
// =========================================================
struct SIMDetectionConfig {
  enum class CardDetection : uint8_t {
    DISABLE = 0,
    ENABLE = 1,
  };

  enum class InsertLevel : uint8_t {
    LOW_LEVEL = 0,
    HIGH_LEVEL = 1,
  };

  CardDetection cardDetection{CardDetection::DISABLE};
  InsertLevel insertLevel{InsertLevel::LOW_LEVEL};
};

// =========================================================
// SIM Status Reporting
// =========================================================
struct SIMStatusReport {
  enum class ReportState : uint8_t {
    DISABLE = 0,
    ENABLE = 1,
  };

  enum class InsertStatus : uint8_t {
    REMOVED = 0,
    INSERTED = 1,
    UNKNOWN = 2,
  };

  ReportState enable{ReportState::DISABLE};
  InsertStatus inserted{InsertStatus::UNKNOWN};
};

// =========================================================
// SIM Slot Selection
// =========================================================
enum class SIMSlot : uint8_t {
  UNKNOWN = 255,
  SLOT_1 = 0,
  SLOT_2 = 1,
};

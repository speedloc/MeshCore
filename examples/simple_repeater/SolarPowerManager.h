#pragma once

#include <Arduino.h>
#include "MyMesh.h"

class SolarPowerManager {
public:
  void begin(FILESYSTEM* fs, MyMesh* mesh);
  void loop();

private:
  static constexpr uint16_t SHUTDOWN_MV = 3200;
  static constexpr uint16_t RESTART_MV = 3400;
  static constexpr uint32_t CHECK_INTERVAL_MS = 3600000UL;
  static constexpr uint32_t STATUS_SEND_WAIT_MS = 8000UL;
  static constexpr uint32_t RECOVERY_RETRY_MS = 30000UL;
  static constexpr uint32_t RECOVERY_START_DELAY_MS = 15000UL;
  static constexpr uint32_t VALID_UNIX_TIME = 1700000000UL;
  static constexpr char STATUS_CHANNEL[] = "#lkgr-info";
  static constexpr char MARKER_FILE[] = "/lowbat_shutdown_v2";
  static constexpr uint32_t MARKER_MAGIC = 0x534F4C52UL; // "SOLR"

  struct LowBatteryMarker {
    uint32_t magic;
    uint32_t shutdown_timestamp;
    uint16_t shutdown_mv;
    uint16_t reserved;
  };

  FILESYSTEM* _fs = nullptr;
  MyMesh* _mesh = nullptr;
  uint32_t _last_check_ms = 0;
  uint32_t _next_recovery_try_ms = 0;
  bool _recovery_pending = false;
  LowBatteryMarker _marker{};

  bool writeMarker(uint16_t battery_mv, uint32_t timestamp);
  bool loadMarker();
  void clearMarker();
  void waitForTransmission();
  void checkLowBattery();
  void tryRecoveryMessage();
  void formatOfflineDuration(char* out, size_t out_size, uint32_t seconds) const;
};

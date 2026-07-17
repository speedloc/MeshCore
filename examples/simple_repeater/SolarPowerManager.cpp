#include "SolarPowerManager.h"
#include <target.h>

constexpr char SolarPowerManager::STATUS_CHANNEL[];
constexpr char SolarPowerManager::MARKER_FILE[];

void SolarPowerManager::begin(FILESYSTEM* fs, MyMesh* mesh) {
  _fs = fs;
  _mesh = mesh;
  // Perform the first battery check immediately after startup.
  _last_check_ms = millis() - CHECK_INTERVAL_MS;
  _next_recovery_try_ms = millis() + RECOVERY_START_DELAY_MS;
  _recovery_pending = loadMarker();

  if (_recovery_pending) {
    MESH_DEBUG_PRINTLN("SOLAR: recovery marker loaded (%u mV, timestamp=%lu)",
      _marker.shutdown_mv, static_cast<unsigned long>(_marker.shutdown_timestamp));
  }
}

void SolarPowerManager::loop() {
#if defined(XIAO_NRF52) && defined(NRF52_POWER_MANAGEMENT)
  if (_recovery_pending) {
    tryRecoveryMessage();
  }

  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - _last_check_ms) >= CHECK_INTERVAL_MS) {
    _last_check_ms = now;
    checkLowBattery();
  }
#endif
}

bool SolarPowerManager::writeMarker(uint16_t battery_mv, uint32_t timestamp) {
  if (!_fs) return false;

  LowBatteryMarker marker{};
  marker.magic = MARKER_MAGIC;
  marker.shutdown_timestamp = timestamp;
  marker.shutdown_mv = battery_mv;

  if (_fs->exists(MARKER_FILE)) {
    _fs->remove(MARKER_FILE);
  }

  File f = _fs->open(MARKER_FILE, FILE_O_WRITE);
  if (!f) return false;
  const size_t written = f.write(reinterpret_cast<const uint8_t*>(&marker), sizeof(marker));
  f.flush();
  f.close();
  return written == sizeof(marker);
}

bool SolarPowerManager::loadMarker() {
  if (!_fs || !_fs->exists(MARKER_FILE)) return false;

  File f = _fs->open(MARKER_FILE, FILE_O_READ);
  if (!f) return false;
  const size_t read_len = f.read(reinterpret_cast<uint8_t*>(&_marker), sizeof(_marker));
  f.close();

  if (read_len != sizeof(_marker) || _marker.magic != MARKER_MAGIC ||
      _marker.shutdown_mv < 1000 || _marker.shutdown_mv > 5000) {
    MESH_DEBUG_PRINTLN("SOLAR: invalid recovery marker removed");
    clearMarker();
    return false;
  }
  return true;
}

void SolarPowerManager::clearMarker() {
  if (_fs && _fs->exists(MARKER_FILE)) {
    _fs->remove(MARKER_FILE);
  }
  _recovery_pending = false;
  memset(&_marker, 0, sizeof(_marker));
}

void SolarPowerManager::waitForTransmission() {
  const uint32_t started = millis();
  while (static_cast<uint32_t>(millis() - started) < STATUS_SEND_WAIT_MS) {
    _mesh->loop();
    sensors.loop();
    rtc_clock.tick();
    delay(10);
  }
}

const char* SolarPowerManager::nodeName() const {
  if (_mesh) {
    const char* configured = _mesh->getNodeName();
    if (configured && configured[0] != '\0') return configured;
  }
  return "Solar Repeater";
}

void SolarPowerManager::checkLowBattery() {
  // USB power always keeps the board awake for maintenance and updates.
  if (board.isExternalPowered()) return;

  const uint16_t battery_mv = board.getBattMilliVolts();
  if (battery_mv <= 1000 || battery_mv > SHUTDOWN_MV) return;

  const uint32_t timestamp = _mesh->getRTCClock()->getCurrentTime();
  if (!writeMarker(battery_mv, timestamp)) {
    MESH_DEBUG_PRINTLN("SOLAR: warning - could not persist shutdown marker");
  }

  char status[128];
  snprintf(status, sizeof(status),
           "%s\nAkku %.2f V, Deep-Sleep bis %.2f V",
           nodeName(), battery_mv / 1000.0f, RESTART_MV / 1000.0f);

  if (_mesh->sendHashtagStatus(STATUS_CHANNEL, status)) {
    MESH_DEBUG_PRINTLN("SOLAR: shutdown message queued, waiting for TX");
    waitForTransmission();
  } else {
    MESH_DEBUG_PRINTLN("SOLAR: shutdown message could not be queued");
  }

  radio_driver.powerOff();
  Serial.flush();
  delay(100);
  board.lowBatteryShutdown(); // does not return
}

void SolarPowerManager::formatOfflineDuration(char* out, size_t out_size, uint32_t seconds) const {
  const uint32_t days = seconds / 86400UL;
  seconds %= 86400UL;
  const uint32_t hours = seconds / 3600UL;
  const uint32_t minutes = (seconds % 3600UL) / 60UL;

  if (days > 0) {
    snprintf(out, out_size, "%lud %02luh %02lum",
             static_cast<unsigned long>(days),
             static_cast<unsigned long>(hours),
             static_cast<unsigned long>(minutes));
  } else {
    snprintf(out, out_size, "%luh %02lum",
             static_cast<unsigned long>(hours),
             static_cast<unsigned long>(minutes));
  }
}

void SolarPowerManager::tryRecoveryMessage() {
  const uint32_t now_ms = millis();
  if (static_cast<int32_t>(now_ms - _next_recovery_try_ms) < 0) return;
  _next_recovery_try_ms = now_ms + RECOVERY_RETRY_MS;

  const uint32_t now = _mesh->getRTCClock()->getCurrentTime();

  const uint16_t current_mv = board.getBattMilliVolts();
  char status[128];

  if (_marker.shutdown_timestamp >= VALID_UNIX_TIME && now >= _marker.shutdown_timestamp) {
    char offline[24];
    formatOfflineDuration(offline, sizeof(offline), now - _marker.shutdown_timestamp);
    snprintf(status, sizeof(status),
             "%s\nWieder online, Akku %.2f V, Abschaltung bei %.2f V, offline seit %s",
             nodeName(), current_mv / 1000.0f,
             _marker.shutdown_mv / 1000.0f, offline);
  } else if (now < VALID_UNIX_TIME ||
             (_marker.shutdown_timestamp >= VALID_UNIX_TIME &&
              now < _marker.shutdown_timestamp)) {
    snprintf(status, sizeof(status),
             "%s\nWieder online, Akku %.2f V, Bitte Uhrzeit einstellen!",
             nodeName(), current_mv / 1000.0f);
  } else {
    // The current clock is valid, but shutdown happened without a valid time.
    snprintf(status, sizeof(status),
             "%s\nWieder online, Akku %.2f V, Abschaltung bei %.2f V, offline unbekannt",
             nodeName(), current_mv / 1000.0f,
             _marker.shutdown_mv / 1000.0f);
  }

  if (_mesh->sendHashtagStatus(STATUS_CHANNEL, status)) {
    MESH_DEBUG_PRINTLN("SOLAR: recovery message queued, waiting for TX");
    waitForTransmission();
    clearMarker();
    MESH_DEBUG_PRINTLN("SOLAR: recovery marker cleared");
  } else {
    MESH_DEBUG_PRINTLN("SOLAR: recovery message queue failed, retrying later");
  }
}

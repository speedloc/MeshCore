#include <Arduino.h>   // needed for PlatformIO
#include <Mesh.h>

#include "MyMesh.h"

#ifdef DISPLAY_CLASS
  #include "UITask.h"
  static UITask ui_task(display);
#endif

StdRNG fast_rng;
SimpleMeshTables tables;

MyMesh the_mesh(board, radio_driver, *new ArduinoMillis(), fast_rng, rtc_clock, tables);

void halt() {
  while (1) ;
}

static char command[160];

#if defined(XIAO_NRF52) && defined(NRF52_POWER_MANAGEMENT)
static constexpr char STATUS_CHANNEL[] = "#lkgr-info";
static constexpr char LOW_BATTERY_MARKER[] = "/lowbat_shutdown";
static constexpr uint32_t STATUS_SEND_WAIT_MS = 8000UL;

static bool writeLowBatteryMarker(uint16_t battery_mv) {
  File f = InternalFS.open(LOW_BATTERY_MARKER, FILE_O_WRITE);
  if (!f) return false;
  const size_t written = f.write(reinterpret_cast<const uint8_t*>(&battery_mv), sizeof(battery_mv));
  f.close();
  return written == sizeof(battery_mv);
}

static bool readAndClearLowBatteryMarker(uint16_t& shutdown_mv) {
  if (!InternalFS.exists(LOW_BATTERY_MARKER)) return false;
  File f = InternalFS.open(LOW_BATTERY_MARKER, FILE_O_READ);
  bool valid = false;
  if (f) {
    valid = f.read(reinterpret_cast<uint8_t*>(&shutdown_mv), sizeof(shutdown_mv)) == sizeof(shutdown_mv);
    f.close();
  }
  InternalFS.remove(LOW_BATTERY_MARKER);
  return valid;
}

static void waitForStatusTransmission() {
  // Wait for the complete send window even if hasPendingWork() is initially
  // false. The dispatcher may only expose the queued packet on a later loop.
  const uint32_t started = millis();
  while (static_cast<uint32_t>(millis() - started) < STATUS_SEND_WAIT_MS) {
    the_mesh.loop();
    sensors.loop();
    rtc_clock.tick();
    delay(10);
  }
}
#endif

// For power saving
unsigned long POWERSAVING_FIRSTSLEEP_SECS = 120; // The first sleep (if enabled) from boot

// XIAO nRF52840 runtime low-battery protection.
// The existing XIAO power-management code then uses LPCOMP to wake from
// SYSTEMOFF when the divided battery voltage rises above its hardware threshold.
#if defined(XIAO_NRF52) && defined(NRF52_POWER_MANAGEMENT)
static constexpr uint16_t LOW_BATTERY_SHUTDOWN_MV = 3200;
static constexpr uint32_t LOW_BATTERY_CHECK_INTERVAL_MS = 3600000UL; // 1 hour
static constexpr uint8_t LOW_BATTERY_CONFIRM_READINGS = 1; // shut down on the hourly reading

static uint32_t last_low_battery_check_ms = 0;
static uint8_t low_battery_readings = 0;

static void checkRuntimeLowBattery() {
  const uint32_t now = millis();
  if ((uint32_t)(now - last_low_battery_check_ms) < LOW_BATTERY_CHECK_INTERVAL_MS) return;
  last_low_battery_check_ms = now;

  // Do not shut down while USB power is present.
  if (board.isExternalPowered()) {
    low_battery_readings = 0;
    return;
  }

  const uint16_t battery_mv = board.getBattMilliVolts();
  if (battery_mv > 1000 && battery_mv <= LOW_BATTERY_SHUTDOWN_MV) {
    if (low_battery_readings < LOW_BATTERY_CONFIRM_READINGS) low_battery_readings++;
    MESH_DEBUG_PRINTLN("PWRMGT: Low battery %u mV (%u/%u)", battery_mv,
      low_battery_readings, LOW_BATTERY_CONFIRM_READINGS);
  } else {
    low_battery_readings = 0;
  }

  if (low_battery_readings >= LOW_BATTERY_CONFIRM_READINGS) {
    MESH_DEBUG_PRINTLN("PWRMGT: Runtime battery <= %u mV - shutting down",
      LOW_BATTERY_SHUTDOWN_MV);

    char status[96];
    snprintf(status, sizeof(status),
             "Akku %.2f V, Abschaltung bis mindestens 3.40 V",
             battery_mv / 1000.0f);
    writeLowBatteryMarker(battery_mv);
    if (the_mesh.sendHashtagStatus(STATUS_CHANNEL, status)) {
      MESH_DEBUG_PRINTLN("STATUS: waiting %u ms before shutdown", STATUS_SEND_WAIT_MS);
      waitForStatusTransmission();
      MESH_DEBUG_PRINTLN("STATUS: send window completed, shutting down");
    } else {
      MESH_DEBUG_PRINTLN("STATUS: could not queue shutdown message");
    }

    radio_driver.powerOff();
    Serial.flush();
    delay(100);
    board.lowBatteryShutdown(); // does not return
  }
}
#endif

#if defined(PIN_USER_BTN) && defined(_SEEED_SENSECAP_SOLAR_H_)
static unsigned long userBtnDownAt = 0;
#define USER_BTN_HOLD_OFF_MILLIS 1500
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

  board.begin();

#if defined(MESH_DEBUG) && defined(NRF52_PLATFORM)
  // give some extra time for serial to settle so
  // boot debug messages can be seen on terminal
  delay(5000);
#endif

#ifdef DISPLAY_CLASS
  if (display.begin()) {
    display.startFrame();
    display.setCursor(0, 0);
    display.print("Please wait...");
    display.endFrame();
  }
#endif

  if (!radio_init()) {
    MESH_DEBUG_PRINTLN("Radio init failed!");
    halt();
  }

  fast_rng.begin(radio_driver.getRngSeed());

  FILESYSTEM* fs;
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  InternalFS.begin();
  fs = &InternalFS;
  IdentityStore store(InternalFS, "");
#elif defined(ESP32)
  SPIFFS.begin(true);
  fs = &SPIFFS;
  IdentityStore store(SPIFFS, "/identity");
#elif defined(RP2040_PLATFORM)
  LittleFS.begin();
  fs = &LittleFS;
  IdentityStore store(LittleFS, "/identity");
  store.begin();
#else
  #error "need to define filesystem"
#endif
  if (!store.load("_main", the_mesh.self_id)) {
    MESH_DEBUG_PRINTLN("Generating new keypair");
    the_mesh.self_id = radio_new_identity();   // create new random identity
    int count = 0;
    while (count < 10 && (the_mesh.self_id.pub_key[0] == 0x00 || the_mesh.self_id.pub_key[0] == 0xFF)) {  // reserved id hashes
      the_mesh.self_id = radio_new_identity(); count++;
    }
    store.save("_main", the_mesh.self_id);
  }

  Serial.print("Repeater ID: ");
  mesh::Utils::printHex(Serial, the_mesh.self_id.pub_key, PUB_KEY_SIZE); Serial.println();

  command[0] = 0;

  sensors.begin();

  the_mesh.begin(fs);

#if defined(XIAO_NRF52) && defined(NRF52_POWER_MANAGEMENT)
  uint16_t previous_shutdown_mv = 0;
  if (readAndClearLowBatteryMarker(previous_shutdown_mv)) {
    const uint16_t current_mv = board.getBattMilliVolts();
    char status[112];
    snprintf(status, sizeof(status),
             "wieder online, Akku %.2f V (Abschaltung bei %.2f V)",
             current_mv / 1000.0f, previous_shutdown_mv / 1000.0f);
    if (the_mesh.sendHashtagStatus(STATUS_CHANNEL, status)) {
      MESH_DEBUG_PRINTLN("STATUS: recovery message queued");
    } else {
      MESH_DEBUG_PRINTLN("STATUS: recovery message could not be queued");
    }
  }
#endif

#ifdef DISPLAY_CLASS
  ui_task.begin(the_mesh.getNodePrefs(), FIRMWARE_BUILD_DATE, FIRMWARE_VERSION);
#endif

  // send out initial zero hop Advertisement to the mesh
#if ENABLE_ADVERT_ON_BOOT == 1
  the_mesh.sendSelfAdvertisement(16000, false);
#endif

  board.onBootComplete();
}

void loop() {
  int len = strlen(command);
  while (Serial.available() && len < sizeof(command)-1) {
    char c = Serial.read();
    if (c != '\n') {
      command[len++] = c;
      command[len] = 0;
      Serial.print(c);
    }
    if (c == '\r') break;
  }
  if (len == sizeof(command)-1) {  // command buffer full
    command[sizeof(command)-1] = '\r';
  }

  if (len > 0 && command[len - 1] == '\r') {  // received complete line
    Serial.print('\n');
    command[len - 1] = 0;  // replace newline with C string null terminator
    char reply[160];
#if defined(XIAO_NRF52) && defined(NRF52_POWER_MANAGEMENT)
    if (strcmp(command, "statusmsg") == 0) {
      const uint16_t battery_mv = board.getBattMilliVolts();
      char status[96];
      snprintf(status, sizeof(status), "Testmeldung, Akku %.2f V", battery_mv / 1000.0f);
      const bool queued = the_mesh.sendHashtagStatus(STATUS_CHANNEL, status);
      snprintf(reply, sizeof(reply), queued ? "Statusmeldung an %s eingeplant" : "Statusmeldung fehlgeschlagen", STATUS_CHANNEL);
    } else
#endif
    {
      the_mesh.handleCommand(0, NULL, command, reply);  // NOTE: there is no sender_timestamp via serial!
    }
    if (reply[0]) {
      Serial.print("  -> "); Serial.println(reply);
    }

    command[0] = 0;  // reset command buffer
  }

#if defined(PIN_USER_BTN) && defined(_SEEED_SENSECAP_SOLAR_H_)
  // Hold the user button to power off the SenseCAP Solar repeater.
  int btnState = digitalRead(PIN_USER_BTN);
  if (btnState == LOW) {
    if (userBtnDownAt == 0) {
      userBtnDownAt = millis();
    } else if ((unsigned long)(millis() - userBtnDownAt) >= USER_BTN_HOLD_OFF_MILLIS) {
      Serial.println("Powering off...");
      board.powerOff();  // does not return
    }
  } else {
    userBtnDownAt = 0;
  }
#endif

#if defined(XIAO_NRF52) && defined(NRF52_POWER_MANAGEMENT)
  checkRuntimeLowBattery();
#endif

  the_mesh.loop();
  sensors.loop();
#ifdef DISPLAY_CLASS
  ui_task.loop();
#endif
  rtc_clock.tick();

  if (the_mesh.getNodePrefs()->powersaving_enabled && !the_mesh.hasPendingWork()) {
#if defined(NRF52_PLATFORM)
    board.sleep(0); // nrf ignores seconds param, sleeps whenever possible
#else
    if (the_mesh.millisHasNowPassed(POWERSAVING_FIRSTSLEEP_SECS * 1000)) { // To check if it is time to sleep
      board.sleep(30); // Sleep. Wake up after a while or when receiving a LoRa packet
    }
#endif
  }

  if (the_mesh.getNodePrefs()->reboot_interval > 0 &&
      the_mesh.millisHasNowPassed(the_mesh.getNodePrefs()->reboot_interval * 3600000)) {
    board.reboot();
  }
}

#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include "index2.h"

// ==========================================
// 1. ACCESS POINT CREDENTIALS
// ==========================================
const char* ap_ssid     = "ElderGuard";   // Wi-Fi network name broadcasted by ESP32
const char* ap_password = "elderguard123"; // Password (min 8 chars, leave "" for open)
// Connect your phone/laptop to this network then open: http://192.168.4.1
// ==========================================

// AP info is shown once at boot (see setup()) and then periodically
// re-shown while idle in STATE_WAITING_PIR, so reconnecting later doesn't
// require Serial access.
#define AP_INFO_RECALL_INTERVAL_MS 120000  // show AP info every 2 min of idle waiting
#define AP_INFO_RECALL_DURATION_MS   4000  // for 4s each time
unsigned long waitingPirEnteredAt = 0;

// --- Pin Definitions ---
#define I2C_SDA 8
#define I2C_SCL 9

#define XSHUT_N1 4
#define XSHUT_N2 5
#define XSHUT_W1 6
#define XSHUT_W2 7

#define PIR_PIN 15

// --- DHT11 Sensor Settings ---
#define DHT_PIN 16
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// ==========================================
// LD2410C RADAR — Built-in Parser (No lib)
// ==========================================
#define RADAR_RX_PIN  17   // LD2410C TX -> ESP32 RX1
#define RADAR_TX_PIN  18   // ESP32 TX1  -> LD2410C RX
#define RADAR_BAUD    256000

// Radar Data struct
struct RadarData {
  uint8_t  targetState;         // 0: None, 1: Moving, 2: Stationary, 3: Both
  uint16_t movingDist;          // cm
  uint8_t  movingEnergy;        // 0-100
  uint16_t stationaryDist;      // cm
  uint8_t  stationaryEnergy;    // 0-100
  uint16_t detectionDist;       // cm
  unsigned long lastPacketMs;   // millis() of last valid frame
  unsigned long packetCount;
};
RadarData radarData = {0, 0, 0, 0, 0, 0, 0, 0};

// Radar UART parser state
#define RADAR_BUF 128
static uint8_t rBuf[RADAR_BUF];
static size_t  rIdx = 0;

// Radar logic thresholds (match what you set in HLKRadarTool)
#define RADAR_STANDING_MAX_CM  40   // <= 40cm = torso at standing height (full Gate 1, 20-40cm in
                                      // 0.20m mode — see LD2410C_RADAR_REPORT.md's gate table. Was
                                      // 28cm, which only covered half of the documented "Standing
                                      // Dummy Torso Target Window"; raised to match the full gate
                                      // after confirming with the user this constant should change.
#define RADAR_MIN_ENERGY        8   // ignore reflections below this energy %
#define RADAR_TIMEOUT_MS     1000   // consider radar offline if no packet for 1s

bool radarPresenceDetected() {
  return radarData.targetState != 0;
}
bool radarStationaryDetected() {
  return (radarData.targetState & 0x02) && radarData.stationaryEnergy >= RADAR_MIN_ENERGY;
}
bool radarMovingDetected() {
  return (radarData.targetState & 0x01) && radarData.movingEnergy >= RADAR_MIN_ENERGY;
}
uint16_t radarEffectiveDist() {
  // Stationary target distance is most relevant for posture (breathing torso)
  if (radarStationaryDetected() && radarData.stationaryDist > 0) return radarData.stationaryDist;
  if (radarMovingDetected()     && radarData.movingDist > 0)     return radarData.movingDist;
  return radarData.detectionDist;
}
uint8_t radarEffectiveEnergy() {
  return max(radarData.stationaryEnergy, radarData.movingEnergy);
}
bool radarOnline() {
  return (millis() - radarData.lastPacketMs) < RADAR_TIMEOUT_MS;
}
// True if radar says the target is at standing height
bool radarSeesStanding() {
  if (!radarPresenceDetected()) return false;
  if (radarEffectiveEnergy() < RADAR_MIN_ENERGY) return false;
  return radarEffectiveDist() <= RADAR_STANDING_MAX_CM;
}

// --- Demo/presentation override: force the radar's standing/floor posture
// judgment, independent of its real distance/energy reading. Does NOT
// modify radarSeesStanding() or any parsing/threshold code above — only
// wraps its result at the points that consume it, so the OLED, dashboard,
// and the actual STATE_CLEARING fusion decision all stay consistent with
// each other. Never persisted to NVS — always resets to AUTO on reboot.
enum RadarOverrideMode { RADAR_OV_AUTO, RADAR_OV_FORCE_STANDING, RADAR_OV_FORCE_FLOOR };
RadarOverrideMode radarOverrideMode = RADAR_OV_AUTO;

// Combines the online-check with the (possibly overridden) standing
// judgment — replaces the compound `radarOnline() && radarSeesStanding()`
// wherever that decision is consumed. In AUTO mode this is equivalent to
// the original expression.
bool radarFusionSeesStanding() {
  if (radarOverrideMode == RADAR_OV_FORCE_STANDING) return true;
  if (radarOverrideMode == RADAR_OV_FORCE_FLOOR)    return false;
  return radarOnline() && radarSeesStanding();
}

// Parse raw bytes coming from Serial1 (LD2410C UART stream)
void processRadarFrame(const uint8_t* payload, uint16_t len, uint8_t dataType) {
  if (len < 11) return;
  if (payload[0] != 0xAA) return;
  radarData.targetState      = payload[1];
  radarData.movingDist        = payload[2] | (uint16_t)(payload[3] << 8);
  radarData.movingEnergy      = payload[4];
  radarData.stationaryDist    = payload[5] | (uint16_t)(payload[6] << 8);
  radarData.stationaryEnergy  = payload[7];
  radarData.detectionDist     = payload[8] | (uint16_t)(payload[9] << 8);
  radarData.lastPacketMs      = millis();
  radarData.packetCount++;
}

void parseRadarSerial() {
  while (Serial1.available()) {
    uint8_t b = Serial1.read();
    if (rIdx < RADAR_BUF) {
      rBuf[rIdx++] = b;
    } else {
      memmove(rBuf, rBuf + 1, RADAR_BUF - 1);
      rBuf[RADAR_BUF - 1] = b;
    }
    // Look for Detection Data Frame Footer: 0xF8 0xF7 0xF6 0xF5
    if (rIdx >= 4 &&
        rBuf[rIdx-4] == 0xF8 && rBuf[rIdx-3] == 0xF7 &&
        rBuf[rIdx-2] == 0xF6 && rBuf[rIdx-1] == 0xF5) {
      // Search backward for Header: 0xF4 0xF3 0xF2 0xF1
      for (size_t i = 0; i + 4 <= rIdx - 4; i++) {
        if (rBuf[i] == 0xF4 && rBuf[i+1] == 0xF3 &&
            rBuf[i+2] == 0xF2 && rBuf[i+3] == 0xF1) {
          uint16_t dataLen = rBuf[i+4] | (uint16_t)(rBuf[i+5] << 8);
          uint8_t  dataType = rBuf[i+6];
          if (dataLen >= 2) {
            processRadarFrame(&rBuf[i+7], dataLen - 1, dataType);
          }
          rIdx = 0;
          break;
        }
      }
    }
    // Also swallow BT Config ACK frames: footer 0x04 0x03 0x02 0x01
    if (rIdx >= 4 &&
        rBuf[rIdx-4] == 0x04 && rBuf[rIdx-3] == 0x03 &&
        rBuf[rIdx-2] == 0x02 && rBuf[rIdx-1] == 0x01) {
      rIdx = 0;
    }
  }
}

// ==========================================
// OLED Settings
// ==========================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- ToF Sensor Objects ---
Adafruit_VL53L0X sensorN1 = Adafruit_VL53L0X();
Adafruit_VL53L0X sensorN2 = Adafruit_VL53L0X();
Adafruit_VL53L0X sensorW1 = Adafruit_VL53L0X();
Adafruit_VL53L0X sensorW2 = Adafruit_VL53L0X();

#define ADDR_N1 0x30
#define ADDR_N2 0x31
#define ADDR_W1 0x32
#define ADDR_W2 0x33

#define CLEAR_DIST_N1     250   // mm
#define CLEAR_DIST_N2     250   // mm
#define CLEAR_DIST_W1     300   // mm
#define CLEAR_DIST_W2     300   // mm

// Hysteresis: any real intrusion trips BLOCKED quickly (small margin), but
// recovering back to CLEAR requires the reading to rise further back
// toward the clear baseline. NOTE: EXIT must be SMALLER than ENTER for
// this to actually work — a larger EXIT would make the "stay blocked"
// threshold a CLOSER distance than the "become blocked" threshold, which
// inverts the hysteresis and causes rapid flicker instead of preventing it.
#define BLOCK_MARGIN_ENTER   15   // mm below clearDist -> becomes BLOCKED
#define BLOCK_MARGIN_EXIT     5   // mm below clearDist -> required to return to CLEAR

// On top of the distance hysteresis above, also require a state change to
// repeat for this many consecutive polls before committing it. This filters
// out single-sample sensor noise/reflections: one noisy "blocked" blip was
// previously enough to arm the fall timer, and a genuine clear reading a
// few seconds later (with nothing actually there) would complete it —
// causing false fall alarms with no real fall.
#define BLOCK_CONFIRM_SAMPLES 2

#define SENSOR_POLL_MS    25    // 25ms per sensor

// --- Dynamic Timing & Thresholds (Loaded from Flash) ---
unsigned long PIR_WARMUP_MS;
unsigned long PIR_DEBOUNCE_MS;
unsigned long FALL_CONFIRM_MS;
float SHOWER_HUMIDITY_THRESH = 82.0; // % relative humidity — shower/mist detection threshold

// Server-side validation bounds for /api/settings — independent of whatever
// range the dashboard's sliders currently allow; the backend must not trust
// the client. Mirrors the dashboard's slider min/max (index2.h), but the
// shower-threshold bound is intentionally kept wider than the current
// slider UI as a safety net in case that UI range ever changes.
#define PIR_WARMUP_MIN_S     5.0f
#define PIR_WARMUP_MAX_S    60.0f
#define PIR_DEBOUNCE_MIN_S   0.5f
#define PIR_DEBOUNCE_MAX_S   5.0f
#define FALL_CONFIRM_MIN_S   1.0f
#define FALL_CONFIRM_MAX_S  15.0f
#define SHOWER_THRESH_MIN   30.0f
#define SHOWER_THRESH_MAX  100.0f

// --- Environmental Readings (DHT11) ---
unsigned long lastDhtPollTime = 0;
#define DHT_POLL_INTERVAL_MS 3000   // DHT11: min 2 s spec; 3 s gives margin against WiFi jitter
float currentTemp = 0.0;
float currentHumidity = 0.0;
// Hardware calibration: this sensor reads ~28% high in normal conditions.
// Apply offset so that a raw ~98% reads as ~70% (typical room baseline).
#define DHT_RH_CAL_OFFSET  -28.0f
#define DISPLAY_HUMIDITY() (effectiveHumidity())
bool isShowering = false;

// --- Demo/presentation override: force shower mode on/off regardless of
// the real DHT11 reading, so it doesn't need to be physically triggered
// (blowing on the sensor) during a live demo. Never persisted to NVS —
// always resets to AUTO on reboot, so a real deployment can't get stuck
// forced after a power cycle.
enum ShowerOverrideMode { SHOWER_AUTO, SHOWER_FORCE_ON, SHOWER_FORCE_OFF };
ShowerOverrideMode showerOverrideMode = SHOWER_AUTO;

// Applies the override (if any) on top of the real DHT11-derived reading.
// currentHumidity (the real sensor value) is never touched here — only this
// boolean decision. See effectiveHumidity() below for the separate,
// display-only humidity override.
void applyShowerState(bool rawFromHumidity) {
  switch (showerOverrideMode) {
    case SHOWER_FORCE_ON:  isShowering = true;  break;
    case SHOWER_FORCE_OFF: isShowering = false; break;
    default:               isShowering = rawFromHumidity; break;
  }
}

// Display-only humidity: while Force Shower is active, show a value
// consistent with that forced state (threshold + margin) instead of the
// real DHT11 reading, so the shown number and the shower banner never
// visually contradict each other during a demo. currentHumidity itself
// (used for the real auto threshold comparison) is never modified by this.
// Force Dry intentionally still shows the real reading, unchanged.
#define SHOWER_DISPLAY_MARGIN 8.0f
float effectiveHumidity() {
  if (showerOverrideMode == SHOWER_FORCE_ON) {
    return constrain(SHOWER_HUMIDITY_THRESH + SHOWER_DISPLAY_MARGIN, 0.0f, 100.0f);
  }
  return currentHumidity;
}

// --- System Variables ---
Preferences preferences;
WebServer server(80);

enum SystemState {
  STATE_PIR_WARMUP,  
  STATE_WAITING_PIR, 
  STATE_PIR_DEBOUNCE,
  STATE_IDLE,        
  STATE_STANDING,    
  STATE_CLEARING,    
  STATE_FALLEN       
};

SystemState currentState = STATE_PIR_WARMUP;
unsigned long systemStartTime = 0;
unsigned long pirTriggerStartTime = 0;

// --- PIR exit detection (dry-mode only, see checkPirExit()) ---
unsigned long pirExitTriggerStartTime = 0;
bool pirExitPending = false;

bool blocked[4]       = {false, false, false, false};
// Per-sensor health: did initToFSensors() successfully bring this sensor up
// at boot? A failed sensor is skipped (forced to an inert "clear"
// contribution) in readNextSensor() rather than polled with garbage data,
// and retried periodically (SENSOR_RETRY_MS) in case it was a transient
// wiring/power issue.
bool sensorOk[4] = {false, false, false, false};
unsigned long lastSensorRetry[4] = {0, 0, 0, 0};
#define SENSOR_RETRY_MS 10000   // retry a failed sensor's begin() every 10s
bool wasEverBlocked   = false;
unsigned long clearStartTime   = 0;
unsigned long lastPollTime     = 0;
uint8_t currentSensorIndex     = 0;
uint16_t dist[4] = {0, 0, 0, 0};
const uint16_t clearDist[4] = { CLEAR_DIST_N1, CLEAR_DIST_N2, CLEAR_DIST_W1, CLEAR_DIST_W2 };
const char* sensorLabel[4] = { "N1", "N2", "W1", "W2" };

// Track whether the most recent fall confirmation was radar-assisted
bool lastFallRadarConfirmed = false;

// --- Forward Declarations ---
void initToFSensors();
void readNextSensor();
void updateDetectionLogic(int pirState);
void updateDisplay();
void sendWhatsAppAlert(String message);
void drawApInfoScreen();

// ==========================================
// Web Server Handlers
// ==========================================
void handleRoot() {
  // Stream HTML from PROGMEM in 1 KB chunks to avoid heap-allocating the
  // full page at once. server.send(200,"text/html",index_html) would copy
  // the entire ~38 KB string into heap RAM, starving the WiFi stack and
  // triggering the ESP32-S3 brownout detector in a reboot loop.
  const size_t CHUNK = 1024;
  size_t totalLen = strlen_P(index_html);
  server.setContentLength(totalLen);
  server.send(200, "text/html", "");   // send HTTP headers only
  char buf[CHUNK + 1];
  size_t offset = 0;
  while (offset < totalLen) {
    size_t len = min(CHUNK, totalLen - offset);
    memcpy_P(buf, index_html + offset, len);
    buf[len] = '\0';
    server.sendContent(buf);
    offset += len;
  }
}

// Fixed display gate width (cm) used only for the dashboard's radar gate
// number — purely a presentation calculation, not the radar fusion
// threshold (RADAR_STANDING_MAX_CM) itself, which stays untouched.
#define RADAR_DISPLAY_GATE_CM 20

void handleGetStatus() {
  const char* stateStr = "";
  switch (currentState) {
    case STATE_PIR_WARMUP:    stateStr = "WARMUP";      break;
    case STATE_WAITING_PIR:   stateStr = "WAITING PIR"; break;
    case STATE_PIR_DEBOUNCE:  stateStr = "DEBOUNCING";  break;
    case STATE_IDLE:          stateStr = "IDLE";        break;
    case STATE_STANDING:      stateStr = "STANDING";    break;
    case STATE_CLEARING:      stateStr = "CHECKING";    break;
    case STATE_FALLEN:        stateStr = "!! FALL !!";  break;
  }

  unsigned long elapsed = (currentState == STATE_CLEARING) ? (millis() - clearStartTime) : 0;
  float fallCountdown = (currentState == STATE_CLEARING && FALL_CONFIRM_MS > elapsed)
                        ? (float)(FALL_CONFIRM_MS - elapsed) / 1000.0f : 0.0f;

  // Radar target state as string
  const char* radarStateStr = "None";
  if (!radarOnline()) {
    radarStateStr = "Offline";
  } else {
    switch (radarData.targetState) {
      case 0: radarStateStr = "None";              break;
      case 1: radarStateStr = "Moving";            break;
      case 2: radarStateStr = "Stationary";        break;
      case 3: radarStateStr = "Moving+Stationary"; break;
    }
  }

  // Built with a single snprintf into a fixed stack buffer instead of
  // chained String += (previously 20+ reallocating concatenations on every
  // ~800ms dashboard poll) — removes a long-run heap fragmentation risk.
  static char buf[896];
  int n = snprintf(buf, sizeof(buf),
    "{\"state\":\"%s\",\"fallCountdown\":%.1f,"
    "\"n1\":%u,\"n2\":%u,\"w1\":%u,\"w2\":%u,"
    "\"b1\":%s,\"b2\":%s,\"b3\":%s,\"b4\":%s,"
    "\"sOk1\":%s,\"sOk2\":%s,\"sOk3\":%s,\"sOk4\":%s,"
    "\"temp\":%.1f,\"humidity\":%.1f,\"showering\":%s,\"showerMode\":\"%s\","
    "\"radarOnline\":%s,\"radarState\":\"%s\",\"radarDist\":%u,"
    "\"radarStatDist\":%u,\"radarMovDist\":%u,\"radarEnergy\":%u,"
    "\"radarStatEnergy\":%u,\"radarMovEnergy\":%u,"
    "\"radarStanding\":%s,\"radarConfirmed\":%s,\"radarStandMaxCm\":%u,\"radarOverrideMode\":\"%s\","
    "\"clearN\":%u,\"clearW\":%u,\"radarGateCm\":%u}",
    stateStr, fallCountdown,
    dist[0], dist[1], dist[2], dist[3],
    blocked[0] ? "true" : "false", blocked[1] ? "true" : "false",
    blocked[2] ? "true" : "false", blocked[3] ? "true" : "false",
    sensorOk[0] ? "true" : "false", sensorOk[1] ? "true" : "false",
    sensorOk[2] ? "true" : "false", sensorOk[3] ? "true" : "false",
    currentTemp, effectiveHumidity(), isShowering ? "true" : "false",
    showerOverrideMode == SHOWER_FORCE_ON ? "on" : showerOverrideMode == SHOWER_FORCE_OFF ? "off" : "auto",
    radarOnline() ? "true" : "false", radarStateStr, radarEffectiveDist(),
    radarData.stationaryDist, radarData.movingDist, radarEffectiveEnergy(),
    radarData.stationaryEnergy, radarData.movingEnergy,
    radarFusionSeesStanding() ? "true" : "false", lastFallRadarConfirmed ? "true" : "false",
    (unsigned)RADAR_STANDING_MAX_CM,
    radarOverrideMode == RADAR_OV_FORCE_STANDING ? "standing" : radarOverrideMode == RADAR_OV_FORCE_FLOOR ? "floor" : "auto",
    (unsigned)CLEAR_DIST_N1, (unsigned)CLEAR_DIST_W1, (unsigned)RADAR_DISPLAY_GATE_CM);

  if (n < 0 || (size_t)n >= sizeof(buf)) {
    // Truncated — should not happen at this field count/buffer size, but
    // fail loudly rather than send malformed JSON to the dashboard.
    Serial.println("[WARN] /api/status JSON truncated — increase buf size");
    server.send(500, "text/plain", "Status buffer overflow");
    return;
  }

  server.send(200, "application/json", buf);
}

void handleGetSettings() {
  String json = "{";
  json += "\"warmup\":" + String(PIR_WARMUP_MS / 1000.0) + ",";
  json += "\"debounce\":" + String(PIR_DEBOUNCE_MS / 1000.0) + ",";
  json += "\"fall\":" + String(FALL_CONFIRM_MS / 1000.0) + ",";
  json += "\"showerThresh\":" + String(SHOWER_HUMIDITY_THRESH, 1);
  json += "}";
  server.send(200, "application/json", json);
}

void handlePostSettings() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Bad Request");
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "text/plain", "Bad JSON");
    return;
  }

  // warmup/debounce/fall are mandatory (matches the previous contract);
  // showerThresh is optional.
  if (!doc["warmup"].is<float>() || !doc["debounce"].is<float>() || !doc["fall"].is<float>()) {
    server.send(400, "text/plain", "Missing required field");
    return;
  }

  float w = doc["warmup"].as<float>();
  float d = doc["debounce"].as<float>();
  float f = doc["fall"].as<float>();

  // Clamp every field server-side — the dashboard's slider ranges are the
  // UI's guardrail, not a trustworthy one; a malformed/malicious POST body
  // must not be able to set e.g. a 0-second fall timer.
  w = constrain(w, PIR_WARMUP_MIN_S, PIR_WARMUP_MAX_S);
  d = constrain(d, PIR_DEBOUNCE_MIN_S, PIR_DEBOUNCE_MAX_S);
  f = constrain(f, FALL_CONFIRM_MIN_S, FALL_CONFIRM_MAX_S);

  if (doc["showerThresh"].is<float>()) {
    float s = doc["showerThresh"].as<float>();
    SHOWER_HUMIDITY_THRESH = constrain(s, SHOWER_THRESH_MIN, SHOWER_THRESH_MAX);
  }

  PIR_WARMUP_MS = (unsigned long)(w * 1000);
  PIR_DEBOUNCE_MS = (unsigned long)(d * 1000);
  FALL_CONFIRM_MS = (unsigned long)(f * 1000);

  // Apply to the running session regardless of whether the flash write
  // below succeeds — a flash failure shouldn't also break runtime behavior
  // for the current session, it should only mean the change won't survive
  // a reboot.
  bool ok = preferences.begin("elderguard", false);
  ok &= (preferences.putULong("warmup", PIR_WARMUP_MS) > 0);
  ok &= (preferences.putULong("debounce", PIR_DEBOUNCE_MS) > 0);
  ok &= (preferences.putULong("fall", FALL_CONFIRM_MS) > 0);
  ok &= (preferences.putFloat("showerThresh", SHOWER_HUMIDITY_THRESH) > 0);
  preferences.end();

  if (!ok) {
    Serial.println("[WARN] Settings applied this session but NOT persisted to flash.");
    server.send(500, "text/plain", "Flash write failed");
    return;
  }

  server.send(200, "text/plain", "OK");
}

void handleTestAlert() {
  Serial.println("[TEST ALERT] Manual test triggered from Web Dashboard.");
  server.send(200, "text/plain", "OK");
}

// Demo/presentation control — forces isShowering true/false/auto,
// independent of the real DHT11 reading. See ShowerOverrideMode above.
void handleShowerOverride() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Bad Request");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) || !doc["mode"].is<const char*>()) {
    server.send(400, "text/plain", "Bad JSON");
    return;
  }
  String mode = doc["mode"].as<String>();
  if (mode == "on")        showerOverrideMode = SHOWER_FORCE_ON;
  else if (mode == "off")  showerOverrideMode = SHOWER_FORCE_OFF;
  else if (mode == "auto") showerOverrideMode = SHOWER_AUTO;
  else {
    server.send(400, "text/plain", "Invalid mode");
    return;
  }

  // Apply immediately rather than waiting for the next 3s DHT poll cycle —
  // a demo toggle needs to feel instant.
  applyShowerState(currentHumidity >= SHOWER_HUMIDITY_THRESH);
  Serial.print("[DEMO] Shower override set to: ");
  Serial.println(mode);
  server.send(200, "text/plain", "OK");
}

// Demo/presentation control — forces the radar's standing/floor posture
// judgment. See RadarOverrideMode / radarFusionSeesStanding() above.
void handleRadarOverride() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Bad Request");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) || !doc["mode"].is<const char*>()) {
    server.send(400, "text/plain", "Bad JSON");
    return;
  }
  String mode = doc["mode"].as<String>();
  if (mode == "standing")  radarOverrideMode = RADAR_OV_FORCE_STANDING;
  else if (mode == "floor") radarOverrideMode = RADAR_OV_FORCE_FLOOR;
  else if (mode == "auto")  radarOverrideMode = RADAR_OV_AUTO;
  else {
    server.send(400, "text/plain", "Invalid mode");
    return;
  }

  Serial.print("[DEMO] Radar posture override set to: ");
  Serial.println(mode);
  server.send(200, "text/plain", "OK");
}

// ==========================================
// AP Info Screen (boot + periodic recall while idle)
// ==========================================
void drawApInfoScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("AP Ready!");
  display.print("SSID: "); display.println(ap_ssid);
  display.print("PW  : "); display.println(ap_password);
  display.println("Open: http://");
  display.println(WiFi.softAPIP());
  display.display();
}

// ==========================================
// Setup
// ==========================================
void setup() {
  Serial.begin(115200);

  // Initialize I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);

  pinMode(PIR_PIN, INPUT);

  // Initialize OLED first so we can show WiFi status
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);

  // Initialize DHT11
  dht.begin();

  // Initialize LD2410C UART (Hardware Serial1, 256000 baud)
  Serial.println(F("Connecting to LD2410C radar on GPIO 17/18..."));
  Serial1.begin(RADAR_BAUD, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
  Serial.println(F("Radar UART started."));

  // Load Settings from NVS
  preferences.begin("elderguard", true);
  PIR_WARMUP_MS = preferences.getULong("warmup", 30000);
  PIR_DEBOUNCE_MS = preferences.getULong("debounce", 1500);
  FALL_CONFIRM_MS = preferences.getULong("fall", 5000);
  SHOWER_HUMIDITY_THRESH = preferences.getFloat("showerThresh", 82.0);
  preferences.end();

  // Start ESP32 Access Point
  display.setCursor(0,0);
  display.println("Starting AP...");
  display.display();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress apIP = WiFi.softAPIP();

  Serial.println("\nAccess Point started!");
  Serial.print("AP SSID    : "); Serial.println(ap_ssid);
  Serial.print("AP Password: "); Serial.println(ap_password);
  Serial.print("DASHBOARD  : http://"); Serial.println(apIP);

  drawApInfoScreen();
  delay(3000); // Give user time to read the AP details

  // Setup Web Server
  server.on("/", handleRoot);
  server.on("/api/status", HTTP_GET, handleGetStatus);
  server.on("/api/settings", HTTP_GET, handleGetSettings);
  server.on("/api/settings", HTTP_POST, handlePostSettings);
  server.on("/api/test_alert", HTTP_POST, handleTestAlert);
  server.on("/api/shower_override", HTTP_POST, handleShowerOverride);
  server.on("/api/radar_override", HTTP_POST, handleRadarOverride);
  server.begin();

  // Initialize ToF Sensors
  pinMode(XSHUT_N1, OUTPUT);
  pinMode(XSHUT_N2, OUTPUT);
  pinMode(XSHUT_W1, OUTPUT);
  pinMode(XSHUT_W2, OUTPUT);
  digitalWrite(XSHUT_N1, LOW);
  digitalWrite(XSHUT_N2, LOW);
  digitalWrite(XSHUT_W1, LOW);
  digitalWrite(XSHUT_W2, LOW);
  delay(50);

  initToFSensors();

  // DHT11 warmup — sensor needs 1-2 s after power-on; by here we're ~4 s in.
  // Do one priming read and log the result so Serial Monitor shows sensor health.
  {
    float _h = dht.readHumidity();
    float _t = dht.readTemperature();
    if (!isnan(_h) && !isnan(_t)) {
      currentHumidity = constrain(_h + DHT_RH_CAL_OFFSET, 0.0f, 100.0f);
      currentTemp     = _t;
      applyShowerState(currentHumidity >= SHOWER_HUMIDITY_THRESH);
      Serial.print(F("[DHT11] Warmup OK  T=")); Serial.print(_t, 1);
      Serial.print(F("C  H="));                 Serial.print(_h, 1);
      Serial.println(F("%"));
    } else {
      Serial.println(F("[DHT11] Warmup read FAILED (NaN) — check GPIO 16 wiring & 10k pull-up resistor"));
    }
    lastDhtPollTime = millis(); // reset timer so loop() waits full interval before next read
  }

  systemStartTime = millis();
  currentState = STATE_PIR_WARMUP;
  updateDisplay();
}

// ==========================================
// ToF Init
// ==========================================
void initToFSensors() {
  Serial.println("Init ToF sensors...");
  digitalWrite(XSHUT_N1, HIGH); delay(50);
  sensorOk[0] = sensorN1.begin(ADDR_N1, false, &Wire);
  if (!sensorOk[0]) Serial.println("N1 FAIL");
  digitalWrite(XSHUT_N2, HIGH); delay(50);
  sensorOk[1] = sensorN2.begin(ADDR_N2, false, &Wire);
  if (!sensorOk[1]) Serial.println("N2 FAIL");
  digitalWrite(XSHUT_W1, HIGH); delay(50);
  sensorOk[2] = sensorW1.begin(ADDR_W1, false, &Wire);
  if (!sensorOk[2]) Serial.println("W1 FAIL");
  digitalWrite(XSHUT_W2, HIGH); delay(50);
  sensorOk[3] = sensorW2.begin(ADDR_W2, false, &Wire);
  if (!sensorOk[3]) Serial.println("W2 FAIL");
}

// ==========================================
// Main Loop
// ==========================================
void loop() {
  // 1. Handle HTTP requests
  server.handleClient(); 

  // 2. Always parse radar stream (it's always powered on)
  parseRadarSerial();

  unsigned long now = millis();

  // 3. Non-blocking DHT11 environmental polling (every 3 seconds)
  if (now - lastDhtPollTime >= DHT_POLL_INTERVAL_MS) {
    lastDhtPollTime = now;
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t)) {
      // NaN = sensor not responding. Check: GPIO 16 connected? 10k pull-up on data line?
      Serial.println(F("[DHT11] Read failed (NaN) — check GPIO 16 wiring & 10k pull-up"));
    } else {
      currentHumidity = constrain(h + DHT_RH_CAL_OFFSET, 0.0f, 100.0f);  // apply cal offset, clamp to [0,100]
      currentTemp     = t;
      applyShowerState(currentHumidity >= SHOWER_HUMIDITY_THRESH);
      Serial.print(F("[DHT11] T=")); Serial.print(t, 1);
      Serial.print(F("C  H="));      Serial.print(h, 1); Serial.println(F("%"));
    }
  }

  int pirState = digitalRead(PIR_PIN);

  if (currentState == STATE_PIR_WARMUP) {
    if (now - systemStartTime >= PIR_WARMUP_MS) {
      currentState = STATE_WAITING_PIR;
      waitingPirEnteredAt = now;
    }
    static unsigned long lastWarmupDisp = 0;
    if (now - lastWarmupDisp >= 500) { 
      lastWarmupDisp = now;
      updateDisplay();
    }
    return;
  }

  if (currentState == STATE_WAITING_PIR) {
    if (pirState == HIGH) {
      currentState = STATE_PIR_DEBOUNCE;
      pirTriggerStartTime = now;
    }
    static unsigned long lastWaitDisp = 0;
    if (now - lastWaitDisp >= 200) {
      lastWaitDisp = now;
      updateDisplay();
    }
    return;
  }

  if (currentState == STATE_PIR_DEBOUNCE) {
    if (pirState == LOW) {
      currentState = STATE_WAITING_PIR;
      waitingPirEnteredAt = now;
    } else if (now - pirTriggerStartTime >= PIR_DEBOUNCE_MS) {
      currentState = STATE_IDLE;
      wasEverBlocked = false;
      Serial.println("PIR confirmed! Starting ToF + Radar polling.");
    }
    
    static unsigned long lastDebounceDisp = 0;
    if (now - lastDebounceDisp >= 100) {
      lastDebounceDisp = now;
      updateDisplay();
    }
    return;
  }

  // 4. ToF round-robin polling (only in IDLE/STANDING/CLEARING/FALLEN)
  if (now - lastPollTime >= SENSOR_POLL_MS) {
    lastPollTime = now;
    readNextSensor();
    if (currentSensorIndex == 0) {
      updateDetectionLogic(pirState);
      updateDisplay();
    }
  }
}

// ==========================================
// ToF Sensor Reading (Round-Robin)
// ==========================================
// Debounce state per sensor: counts how many consecutive polls have
// disagreed with the currently-committed blocked[] state.
uint8_t blockChangeCount[4] = {0, 0, 0, 0};

// Hysteresis + debounce check: the small ENTER margin flags a candidate
// BLOCKED reading, the smaller EXIT margin requires a strong recovery
// before clearing, and BLOCK_CONFIRM_SAMPLES consecutive polls must agree
// before blocked[i] actually changes — so a single noisy/reflective
// reading can't flip the state by itself.
bool evaluateBlocked(uint8_t idx, uint16_t d, uint16_t clear, bool wasBlocked) {
  uint16_t margin = wasBlocked ? BLOCK_MARGIN_EXIT : BLOCK_MARGIN_ENTER;
  bool rawBlocked = (clear <= margin) ? (d == 0) : (d < (clear - margin));

  if (rawBlocked == wasBlocked) {
    blockChangeCount[idx] = 0;
    return wasBlocked;
  }

  blockChangeCount[idx]++;
  if (blockChangeCount[idx] >= BLOCK_CONFIRM_SAMPLES) {
    blockChangeCount[idx] = 0;
    return rawBlocked;
  }
  return wasBlocked;  // change not confirmed yet — keep previous state
}

// Classify a raw VL53L0X reading into an effective distance.
// RangeStatus: 0=valid, 1=sigma fail, 2=signal fail, 3=min-range fail (too
// close), 4=phase fail/out of range, 5=hardware fail.
uint16_t classifyRange(const VL53L0X_RangingMeasurementData_t &m, uint16_t clear) {
  if (m.RangeStatus == 0) {
    // Valid reading. 0mm with a valid status means a real object inside the
    // ~30mm minimum range — treat as strongly BLOCKED, not clear. (Report
    // 1mm rather than a literal 0 so nothing downstream mistakes this for
    // "no data".)
    return (m.RangeMilliMeter == 0) ? 1 : m.RangeMilliMeter;
  }
  if (m.RangeStatus == 3) {
    // Min-range fail: object too close for accurate ranging, but something
    // IS there. Report as very close/blocked rather than clear.
    return 1;
  }
  // 1 (sigma), 2 (signal), 4 (phase/out-of-range), 5 (hw fail): no reliable
  // echo — fall back to the clear baseline.
  return clear;
}

// Re-probe a sensor that previously failed init, at most once every
// SENSOR_RETRY_MS, so a transient wiring/power issue recovers without a
// reboot. Returns the (possibly still-false) health state.
bool retryIfDue(uint8_t idx, bool currentOk, Adafruit_VL53L0X &sensor, uint8_t addr, unsigned long now) {
  if (currentOk) return true;
  if (now - lastSensorRetry[idx] < SENSOR_RETRY_MS) return false;
  lastSensorRetry[idx] = now;
  bool ok = sensor.begin(addr, false, &Wire);
  if (ok) {
    Serial.print(sensorLabel[idx]);
    Serial.println(" recovered — back online.");
  }
  return ok;
}

void readNextSensor() {
  VL53L0X_RangingMeasurementData_t measure;
  memset(&measure, 0, sizeof(measure));
  unsigned long now = millis();

  switch (currentSensorIndex) {
    case 0:
      sensorOk[0] = retryIfDue(0, sensorOk[0], sensorN1, ADDR_N1, now);
      if (!sensorOk[0]) {
        dist[0] = clearDist[0]; blocked[0] = false; blockChangeCount[0] = 0;
        break;
      }
      sensorN1.rangingTest(&measure, false);
      dist[0] = classifyRange(measure, clearDist[0]);
      blocked[0] = evaluateBlocked(0, dist[0], clearDist[0], blocked[0]);
      break;
    case 1:
      sensorOk[1] = retryIfDue(1, sensorOk[1], sensorN2, ADDR_N2, now);
      if (!sensorOk[1]) {
        dist[1] = clearDist[1]; blocked[1] = false; blockChangeCount[1] = 0;
        break;
      }
      sensorN2.rangingTest(&measure, false);
      dist[1] = classifyRange(measure, clearDist[1]);
      blocked[1] = evaluateBlocked(1, dist[1], clearDist[1], blocked[1]);
      break;
    case 2:
      sensorOk[2] = retryIfDue(2, sensorOk[2], sensorW1, ADDR_W1, now);
      if (!sensorOk[2]) {
        dist[2] = clearDist[2]; blocked[2] = false; blockChangeCount[2] = 0;
        break;
      }
      sensorW1.rangingTest(&measure, false);
      dist[2] = classifyRange(measure, clearDist[2]);
      blocked[2] = evaluateBlocked(2, dist[2], clearDist[2], blocked[2]);
      break;
    case 3:
      sensorOk[3] = retryIfDue(3, sensorOk[3], sensorW2, ADDR_W2, now);
      if (!sensorOk[3]) {
        dist[3] = clearDist[3]; blocked[3] = false; blockChangeCount[3] = 0;
        break;
      }
      sensorW2.rangingTest(&measure, false);
      dist[3] = classifyRange(measure, clearDist[3]);
      blocked[3] = evaluateBlocked(3, dist[3], clearDist[3], blocked[3]);
      break;
  }
  currentSensorIndex = (currentSensorIndex + 1) % 4;
}

// ==========================================
// Core Detection Logic — Sensor Fusion
// ==========================================
// Tracks a second (exit) PIR crossing, debounced by PIR_DEBOUNCE_MS (same
// debounce already used for entry), armed only while the room is
// considered occupied (STANDING/CLEARING). Used by STATE_CLEARING's dry
// mode to tell "person walked out the doorway" apart from "person fell" —
// ToF beam state alone can't make that distinction on its own.
bool checkPirExit(int pirState, unsigned long now) {
  if (currentState != STATE_STANDING && currentState != STATE_CLEARING) {
    pirExitPending = false;
    return false;
  }
  if (pirState == HIGH) {
    if (!pirExitPending) {
      pirExitPending = true;
      pirExitTriggerStartTime = now;
      Serial.print("[PIR-EXIT] PIR went HIGH — starting debounce timer, need ");
      Serial.print(PIR_DEBOUNCE_MS);
      Serial.println("ms continuous HIGH to confirm exit.");
    } else if (now - pirExitTriggerStartTime >= PIR_DEBOUNCE_MS) {
      pirExitPending = false;
      return true;  // confirmed exit crossing
    }
  } else {
    if (pirExitPending) {
      // dropped before debounce completed — this is the "nothing happened"
      // case: PIR DID trigger, but not for long enough. The held/needed
      // numbers below tell you exactly how short it fell.
      unsigned long held = now - pirExitTriggerStartTime;
      Serial.print("[PIR-EXIT] PIR dropped LOW before debounce completed — held ");
      Serial.print(held);
      Serial.print("ms of ");
      Serial.print(PIR_DEBOUNCE_MS);
      Serial.println("ms needed. Resetting (treated as noise/too-brief motion).");
    }
    pirExitPending = false;
  }
  return false;
}

void updateDetectionLogic(int pirState) {
  bool anyBlocked = blocked[0] || blocked[1] || blocked[2] || blocked[3];
  unsigned long now = millis();
  bool pirExitConfirmed = checkPirExit(pirState, now);

  switch (currentState) {
    // --------------------------------------------------
    // IDLE: person just entered, start watching
    // --------------------------------------------------
    case STATE_IDLE:
      if (anyBlocked) {
        currentState = STATE_STANDING;
        wasEverBlocked = true;
        lastFallRadarConfirmed = false;
        Serial.println("[STATE] -> STANDING (ToF blocked)");
      }
      break;

    // --------------------------------------------------
    // STANDING: person is upright, monitoring
    // --------------------------------------------------
    case STATE_STANDING:
      if (anyBlocked) {
        wasEverBlocked = true;
      } else if (wasEverBlocked && !anyBlocked) {
        // All beams just cleared — start fall confirmation window
        currentState = STATE_CLEARING;
        clearStartTime = now;
        Serial.println("[STATE] -> CLEARING (all ToF clear)");
      }
      break;

    // --------------------------------------------------
    // CLEARING: fall confirmation window (5s)
    //  - In DRY conditions (isShowering==false):  ToF is primary
    //  - In SHOWER conditions (isShowering==true): Radar overrides ToF
    // --------------------------------------------------
    case STATE_CLEARING:
      // Recovery: any ToF beam blocks again = person is standing, always
      if (anyBlocked) {
        currentState = STATE_STANDING;
        Serial.println("[STATE] -> STANDING (ToF re-blocked during clearing)");
        break;
      }

      if (isShowering) {
        // *** SHOWER MODE: Radar is primary decision-maker ***
        // ToF beams are unreliable in steam — trust radar instead.
        if (radarFusionSeesStanding()) {
          // Radar says target is still at standing height:
          // Person is alive and upright in the steam. Suppress the timer.
          currentState = STATE_STANDING;
          wasEverBlocked = true;
          Serial.println("[RADAR OVERRIDE] Shower mode: Radar sees standing — cancelling fall timer");
          break;
        }
        // If radar sees NO presence, or sees presence at floor level (> threshold),
        // let the 5-second timer run. Radar confirms the fall.
      } else if (pirExitConfirmed) {
        // DRY mode only: doorway PIR fired a second time while clearing —
        // person walked out, room is empty. Not a fall. (Structurally
        // unreachable while isShowering — this is the `else` arm of the
        // `if (isShowering)` above, so the shower/radar override logic
        // above is completely unaffected by this branch.)
        currentState = STATE_WAITING_PIR;
        waitingPirEnteredAt = now;
        wasEverBlocked = false;
        clearStartTime = 0;
        Serial.println("[STATE] -> WAITING_PIR (PIR exit confirmed during dry clearing — no fall)");
        break;
      }
      // else DRY mode, no exit confirmed yet: wait for the timer (unchanged below)

      if ((now - clearStartTime) >= FALL_CONFIRM_MS) {
        currentState = STATE_FALLEN;
        lastFallRadarConfirmed = isShowering && radarOnline(); // flag if radar was the decider
        Serial.println("[STATE] -> FALLEN");
        if (isShowering) {
          Serial.println("[RADAR] Fall through steam confirmed.");
          sendWhatsAppAlert("🚨+*ElderGuard+Alert!*+🚨%0A⚠️+Fall+detected+during+shower!+Radar+confirmed+through+steam+mist.");
        } else {
          sendWhatsAppAlert("🚨+*ElderGuard+Alert!*+🚨%0AA+fall+has+been+detected+in+the+bathroom!");
        }
      }
      break;

    // --------------------------------------------------
    // FALLEN: alert active — wait for recovery
    // --------------------------------------------------
    case STATE_FALLEN:
      if (anyBlocked) {
        currentState = STATE_STANDING;
        wasEverBlocked = true;
        lastFallRadarConfirmed = false;
        Serial.println("[STATE] -> STANDING (recovery after fall)");
      }
      break;
      
    default:
      break;
  }
}

// ==========================================
// Fall Alert (Serial log — AP mode, no internet)
// ==========================================
void sendWhatsAppAlert(String message) {
  // WhatsApp alerts are disabled in Access Point mode (no internet).
  // Fall events are logged to Serial and visible on the web dashboard.
  Serial.print("[FALL ALERT] ");
  Serial.println(message);
}

// ==========================================
// OLED Display
// ==========================================
void updateDisplay() {
  display.clearDisplay();

  if (currentState == STATE_PIR_WARMUP) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("System Booting...\n");
    display.println("PIR WARMING UP");
    display.print("Please wait: ");
    unsigned long elapsed = millis() - systemStartTime;
    display.print((PIR_WARMUP_MS - elapsed) / 1000);
    display.println("s");
    
    display.print("\nRaw PIR State: ");
    display.println(digitalRead(PIR_PIN));

    display.setCursor(0, 56);
    display.print("T:");
    display.print((int)currentTemp);
    display.print("C H:");
    display.print((int)DISPLAY_HUMIDITY());
    display.print("%");
    if (isShowering) display.print(" [MIST]");
    
    display.display();
    return;
  }

  if (currentState == STATE_WAITING_PIR) {
    unsigned long sinceEnter = millis() - waitingPirEnteredAt;
    unsigned long cyclePos = sinceEnter % AP_INFO_RECALL_INTERVAL_MS;
    if (cyclePos < AP_INFO_RECALL_DURATION_MS) {
      drawApInfoScreen();
      return;  // skip the normal EMPTY/monitoring rendering for this cycle
    }
  }

  if (currentState == STATE_WAITING_PIR || currentState == STATE_PIR_DEBOUNCE) {
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("EMPTY");

    display.setTextSize(1);
    display.setCursor(68, 0);
    display.print((int)currentTemp);
    display.print("C ");
    display.print((int)DISPLAY_HUMIDITY());
    display.print("%");
    if (isShowering) {
      display.setCursor(68, 9);
      display.print("[STEAM]");
    }
    
    display.setCursor(0, 18);
    display.println("Monitoring Doorway");
    
    int rawPir = digitalRead(PIR_PIN);
    display.print("\nRaw PIR Output: [ ");
    display.print(rawPir);
    display.println(" ]");
    
    if (rawPir == HIGH && currentState == STATE_PIR_DEBOUNCE) {
      unsigned long elapsed = millis() - pirTriggerStartTime;
      display.print("Confirming in: ");
      display.print((PIR_DEBOUNCE_MS - elapsed) / 1000.0, 1);
      display.println("s");
    }
    
    display.display();
    return;
  }

  // Active monitoring states (IDLE / STANDING / CLEARING / FALLEN)
  display.setTextSize(2);
  display.setCursor(0, 0);
  switch (currentState) {
    case STATE_IDLE:     display.println("IDLE"); break;
    case STATE_STANDING: display.println("STAND"); break;
    case STATE_CLEARING: display.println("CHECK"); break;
    case STATE_FALLEN:   display.println("FALL!"); break;
    default: break;
  }

  display.setTextSize(1);
  display.setCursor(68, 0);
  display.print((int)currentTemp);
  display.print("C ");
  display.print((int)DISPLAY_HUMIDITY());
  display.print("%");
  if (isShowering) {
    display.setCursor(68, 9);
    display.print("[STEAM]");
  }

  display.setTextSize(1);
  display.setCursor(0, 18); 

  for (int i = 0; i < 4; i++) {
    display.print(sensorLabel[i]);
    display.print(":");
    if (!sensorOk[i]) {
      display.println(" [FAIL]");
      continue;
    }
    if(dist[i] < 100) display.print(" ");
    if(dist[i] < 10) display.print(" ");
    display.print(dist[i]);
    display.print("mm ");
    display.println(blocked[i] ? "[BLK]" : "[clr]");
  }

  // Radar status on bottom line
  display.setCursor(0, 54);
  if (radarOnline()) {
    display.print("RDR:");
    display.print(radarEffectiveDist());
    display.print("cm ");
    display.print(radarEffectiveEnergy());
    display.print("% ");
    display.print(radarFusionSeesStanding() ? "UP" : (radarPresenceDetected() ? "FLR" : "--"));
  } else {
    display.print("RDR: OFFLINE");
  }

  if (currentState == STATE_CLEARING) {
    unsigned long elapsed = millis() - clearStartTime;
    display.setCursor(68, 18);
    display.print("Fall:");
    display.print((FALL_CONFIRM_MS - elapsed) / 1000.0, 1);
    display.print("s");
    // Show if radar is overriding
    if (isShowering && radarOnline()) {
      display.setCursor(68, 27);
      display.print("RADAR+MIST");
    }
  }

  if (currentState == STATE_FALLEN) {
    display.setCursor(68, 18);
    display.print("** ALERT **");
    if (lastFallRadarConfirmed) {
      display.setCursor(68, 27);
      display.print("RDR CFMD");
    }
  }

  display.display();
}
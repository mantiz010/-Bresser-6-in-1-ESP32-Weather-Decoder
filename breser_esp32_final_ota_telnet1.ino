#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <RadioLib.h>

// === USER CONFIG ===
const char* ssid = "mantiz010";
const char* password = "DavidCross010";
const char* mqtt_server = "172.168.1.8";
const char* mqtt_user = "mantiz010";
const char* mqtt_password = "DavidCross010";

// === MQTT TOPICS ===
const String temperatureTopic    = "bresser/sensor/temperature";
const String humidityTopic       = "bresser/sensor/humidity";
const String windGustTopic       = "bresser/sensor/wind_gust";
const String windSpeedTopic      = "bresser/sensor/wind_speed";
const String windDirectionTopic  = "bresser/sensor/wind_direction";
const String rainfallTopic       = "bresser/sensor/rainfall";
const String batteryTopic        = "bresser/sensor/battery";
const String sensorIdTopic       = "bresser/sensor/id";

// === CC1101 PINS ===
#define PIN_CC1101_CS    5
#define PIN_CC1101_GDO0  22
#define PIN_CC1101_GDO2  4
CC1101 radio = new Module(PIN_CC1101_CS, PIN_CC1101_GDO0, RADIOLIB_NC, PIN_CC1101_GDO2);

// === Globals ===
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// === Telnet Server ===
WiFiServer telnetServer(23);
WiFiClient telnetClient;

// === Logging ===
void log(const String& msg) {
  Serial.println(msg);
  if (telnetClient && telnetClient.connected()) {
    telnetClient.println(msg);
  }
}

// === WiFi ===
void setup_wifi() {
  delay(100);
  WiFi.begin(ssid, password);
  log("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  log("WiFi connected: " + WiFi.localIP().toString());
}

// === MQTT ===
void reconnect() {
  while (!mqttClient.connected()) {
    log("[MQTT] Connecting...");
    if (mqttClient.connect("BresserClient", mqtt_user, mqtt_password)) {
      log("[MQTT] Connected!");
    } else {
      log("[MQTT] Failed, rc=" + String(mqttClient.state()));
      delay(5000);
    }
  }
}

// === Bresser Decoder ===
uint16_t lfsrDigest16(const uint8_t* data, size_t length, uint16_t gen, uint16_t key) {
  uint16_t sum = 0;
  for (size_t k = 0; k < length; ++k) {
    uint8_t byteVal = data[k];
    for (int i = 7; i >= 0; --i) {
      if ((byteVal >> i) & 1) sum ^= key;
      key = (key >> 1) ^ (key & 1 ? gen : 0);
    }
  }
  return sum;
}

int addBytes(const uint8_t* data, size_t length) {
  int result = 0;
  for (size_t i = 0; i < length; i++) result += data[i];
  return result;
}

bool decodeBresser6In1Full(const uint8_t* msg, size_t len) {
  if (len < 18) return false;
  uint16_t providedDigest = (msg[0] << 8) | msg[1];
  uint16_t calcDigest = lfsrDigest16(&msg[2], 15, 0x8810, 0x5412);
  if (providedDigest != calcDigest) return false;
  if ((addBytes(&msg[2], 16) & 0xFF) != 0xFF) return false;

  uint32_t id = (msg[2] << 24) | (msg[3] << 16) | (msg[4] << 8) | msg[5];
  bool battery_ok = (msg[13] & 0x02) != 0;

  bool temp_ok = (msg[12] <= 0x99) && ((msg[13] & 0xF0) <= 0x90);
  int temp_raw = ((msg[12] >> 4) * 100) + ((msg[12] & 0x0F) * 10) + (msg[13] >> 4);
  bool temp_sign = (msg[13] >> 3) & 1;
  float temp_c = temp_raw * 0.1f;
  if (temp_sign) temp_c = (temp_raw - 1000) * 0.1f;

  int humidity = (msg[14] >> 4) * 10 + (msg[14] & 0x0F);

  uint8_t w7 = msg[7] ^ 0xFF, w8 = msg[8] ^ 0xFF, w9 = msg[9] ^ 0xFF;
  bool wind_ok = (w7 <= 0x99) && (w8 <= 0x99) && (w9 <= 0x99);
  float wind_gust = 0.0f, wind_avg = 0.0f;
  int wind_dir = 0;

  if (wind_ok) {
    int gust_raw = ((w7 >> 4) * 100) + ((w7 & 0x0F) * 10) + (w8 >> 4);
    wind_gust = gust_raw * 0.1f;
    int wavg_raw = ((w9 >> 4) * 100) + ((w9 & 0x0F) * 10) + (w8 & 0x0F);
    wind_avg = wavg_raw * 0.1f;
    wind_dir = ((msg[10] & 0xF0) >> 4) * 100 + (msg[10] & 0x0F) * 10 + ((msg[11] & 0xF0) >> 4);
  }

  uint8_t r12 = msg[12] ^ 0xFF, r13 = msg[13] ^ 0xFF, r14 = msg[14] ^ 0xFF;
  float rain_mm = 0.0f;
  bool rain_ok = (msg[16] & 1) && (r12 <= 0x99) && (r13 <= 0x99) && (r14 <= 0x99);
  if (rain_ok) {
    int rain_raw = ((r12 >> 4) * 100000) + ((r12 & 0x0F) * 10000) +
                   ((r13 >> 4) * 1000) + ((r13 & 0x0F) * 100) +
                   ((r14 >> 4) * 10) + (r14 & 0x0F);
    rain_mm = rain_raw * 0.1f;
  }

  log("===== SENSOR DATA =====");
  log("Sensor ID       : " + String(id, HEX));
  log("Battery OK      : " + String(battery_ok ? "Yes" : "No"));
  if (temp_ok) {
    log("Temperature (°C): " + String(temp_c));
    log("Humidity (%)    : " + String(humidity));
  }
  if (wind_ok) {
    log("Wind Gust (m/s) : " + String(wind_gust));
    log("Wind Speed (m/s): " + String(wind_avg));
    log("Wind Direction  : " + String(wind_dir));
  }
  if (rain_mm > 0.0f) log("Rainfall (mm)   : " + String(rain_mm));
  log("========================");

  mqttClient.publish(sensorIdTopic.c_str(), String(id, HEX).c_str());
  mqttClient.publish(batteryTopic.c_str(), battery_ok ? "1" : "0");
  if (temp_ok) {
    mqttClient.publish(temperatureTopic.c_str(), String(temp_c).c_str());
    mqttClient.publish(humidityTopic.c_str(), String(humidity).c_str());
  }
  if (wind_ok) {
    mqttClient.publish(windGustTopic.c_str(), String(wind_gust).c_str());
    mqttClient.publish(windSpeedTopic.c_str(), String(wind_avg).c_str());
    mqttClient.publish(windDirectionTopic.c_str(), String(wind_dir).c_str());
  }
  if (rain_mm > 0.0f) {
    mqttClient.publish(rainfallTopic.c_str(), String(rain_mm).c_str());
  }

  return true;
}

bool autoFindBresserSliceNoID(const uint8_t* recvData, size_t len) {
  for (int offset = 0; offset <= 9; offset++) {
    if (offset + 18 > (int)len) break;
    uint8_t msg[18];
    memcpy(msg, &recvData[offset], 18);
    if (decodeBresser6In1Full(msg, 18)) return true;
  }
  return false;
}

// === Setup ===
void setup() {
  Serial.begin(115200);
  setup_wifi();
  mqttClient.setServer(mqtt_server, 1883);
  telnetServer.begin();
  log("Telnet server started on port 23");

  ArduinoOTA.setHostname("bresser-6in1");
  ArduinoOTA.begin();
  log("OTA Ready");

  int state = radio.begin(917.0, 8.22, 57.136417, 270.0, 10, 32);
  if (state != RADIOLIB_ERR_NONE) {
    log("[CC1101] Init failed: " + String(state));
    while (true);
  }
  radio.setCrcFiltering(false);
  radio.fixedPacketLengthMode(27);
  radio.setSyncWord(0xAA, 0x2D, 0, false);
  log("[CC1101] Initialized");
}

// === Loop ===
void loop() {
  if (!mqttClient.connected()) reconnect();
  mqttClient.loop();
  ArduinoOTA.handle();

  if (!telnetClient || !telnetClient.connected()) {
    telnetClient = telnetServer.available();
  }

  uint8_t recvData[27];
  int state = radio.receive(recvData, 27);
  if (state == RADIOLIB_ERR_NONE) {
    autoFindBresserSliceNoID(recvData, 27);
  } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
    log("[CC1101] Receive failed: " + String(state));
  }
}

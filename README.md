# 🌦️ Bresser 6-in-1 ESP32 Weather Decoder

A complete ESP32 project that decodes Bresser 6-in-1 868 MHz weather station data via a CC1101 module, then publishes values to MQTT for integration with Home Assistant. Includes:

- ✅ CC1101 LoRa decoding (RadioLib)
- ✅ WiFi + MQTT publishing
- ✅ Telnet debug interface (mirrors Serial)
- ✅ OTA firmware updates

---

## 📦 Hardware Required

- ESP32 (DevKit or similar)
- CC1101 868 MHz transceiver
- Bresser 6-in-1 weather station
- Power supply (e.g., USB or battery)

---

## ⚙️ Wiring (ESP32 ↔ CC1101)

| CC1101 Pin | ESP32 Pin |
|------------|-----------|
| VCC        | 3.3V      |
| GND        | GND       |
| CSN (CS)   | GPIO 5    |
| GDO0       | GPIO 22   |
| GDO2       | GPIO 4    |
| SCK        | GPIO 18   |
| MOSI       | GPIO 23   |
| MISO       | GPIO 19   |

---

## 🚀 Features

- **OTA**: Update firmware via Arduino IDE or `arduino-cli`
- **Telnet**: Connect to IP on port 23 to see sensor logs (same as serial)
- **MQTT**: Sends temperature, humidity, wind gust/speed/direction, rainfall, battery, and sensor ID

---

## 📡 Home Assistant MQTT Configuration

Paste this under `mqtt:` in your `configuration.yaml`:

```yaml
sensor:
  - name: "Bresser Temperature"
    state_topic: "bresser/sensor/temperature"
    unit_of_measurement: "°C"
    value_template: "{{ value }}"
    device_class: temperature
    state_class: measurement

  - name: "Bresser Humidity"
    state_topic: "bresser/sensor/humidity"
    unit_of_measurement: "%"
    value_template: "{{ value }}"
    device_class: humidity
    state_class: measurement

  - name: "Bresser Wind Gust"
    state_topic: "bresser/sensor/wind_gust"
    unit_of_measurement: "m/s"
    value_template: "{{ value }}"
    state_class: measurement

  - name: "Bresser Wind Speed"
    state_topic: "bresser/sensor/wind_speed"
    unit_of_measurement: "m/s"
    value_template: "{{ value }}"
    state_class: measurement

  - name: "Bresser Wind Direction"
    state_topic: "bresser/sensor/wind_direction"
    unit_of_measurement: "°"
    value_template: "{{ value }}"
    icon: "mdi:compass"

  - name: "Bresser Rainfall"
    state_topic: "bresser/sensor/rainfall"
    unit_of_measurement: "mm"
    value_template: "{{ value }}"
    state_class: measurement

  - name: "Bresser Battery"
    state_topic: "bresser/sensor/battery"
    value_template: >-
      {% if value == "1" %}
        OK
      {% else %}
        LOW
      {% endif %}
    icon: "mdi:battery"

  - name: "Bresser Sensor ID"
    state_topic: "bresser/sensor/id"
    value_template: "{{ value }}"
    icon: "mdi:chip"
```

---

## 🔧 OTA Tips

- The device will show up in Arduino IDE under **Network Ports** as `Bresser 6 in 1`
- You can flash over WiFi once connected to the same LAN

---

## 🛠️ Compilation

**Libraries Required (install via Library Manager):**

- RadioLib
- ArduinoOTA
- PubSubClient
- WiFi

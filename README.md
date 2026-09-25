# Crenova Wi-Fi Meter

Passive LCD observation and browser dashboard for a Crenova MS8233D 6000-count meter. An XIAO ESP32-C6 reads the SDIC SD7501 LCD through a CD74HC4067, decodes the accepted display state, and publishes it to a self-contained browser dashboard.

`LCD SEG/COM → C6 acquisition → numeric/semantic decoder → Wi-Fi/WebSocket → browser dashboard`

## Hardware

- Crenova MS8233D 6000-count meter with SDIC SD7501 LCD driver
- Seeed Studio XIAO ESP32-C6
- CD74HC4067 analog multiplexer

## Status

The firmware acquires the 60-cell logical LCD state, decodes validated numeric digits and decimal points, recognizes the proven semantic cells, and serves an HTTP/WebSocket dashboard. The final validated dashboard also displays the confirmed minus sign.

Unresolved: V versus ohm, F, diode, continuity, OL, current units/prefixes, APO, battery, NCV, and Live. See [docs/MAPPING_STATUS.md](docs/MAPPING_STATUS.md).

## Build and flash

Install the ESP32 Arduino core, then use Arduino CLI with the XIAO ESP32-C6 board definition:

```sh
cp firmware/meter_dashboard/secrets.h.example firmware/meter_dashboard/secrets.h
# Edit secrets.h with local Wi-Fi credentials.
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C6 firmware/meter_dashboard
arduino-cli upload -p /dev/your-port --fqbn esp32:esp32:XIAO_ESP32C6 firmware/meter_dashboard
```

`secrets.h` is intentionally ignored by Git. After a successful station connection, open `http://meter.local/`, or use the IP address printed over serial. The browser reconnects to `/ws` automatically.

## Safety

This project passively observes LCD signals. Develop and validate on a low-voltage bench; no mains testing is required for project development.

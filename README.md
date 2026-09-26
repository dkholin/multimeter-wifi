# Crenova Wi-Fi Meter

Passive LCD observation and browser dashboard for a Crenova MS8233D 6000-count meter. An XIAO ESP32-C6 reads the SDIC SD7501 LCD through a CD74HC4067, decodes the accepted display state, and publishes it to a self-contained browser dashboard.

`LCD SEG/COM → C6 acquisition → numeric/semantic decoder → Wi-Fi/WebSocket → browser dashboard`

## Hardware

- Crenova MS8233D 6000-count meter with SDIC SD7501 LCD driver
- Seeed Studio XIAO ESP32-C6
- CD74HC4067 analog multiplexer

## Status

The firmware acquires the 60-cell logical LCD state, decodes validated numeric digits and decimal points, recognizes the proven semantic cells, and serves an HTTP/WebSocket dashboard. The dashboard also displays the minus sign, V/ohm/F units, diode, continuity and OL.

Unresolved: micro prefix, current units/prefixes, APO, battery, NCV, and Live. See [docs/MAPPING_STATUS.md](docs/MAPPING_STATUS.md).

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

## Dashboard notes

- Browser-side only: function label, graph (30 s / 2 min / 10 min), min/max/avg, and record + CSV export (1 Hz, live readings only, not persisted across reloads).
- If the serial log shows `sync_edge_uncertainty` after rebooting the ESP32, power-cycle the meter; readings resume.

## UNI-T UT61E+ variant

`firmware/ut61eplus_reader/` reads a UNI-T UT61E+ over the D-09A optical link and serves the same dashboard at `http://unit-meter.local/` (build with `pio run -t upload`; copy `src/secrets.h.example` to `src/secrets.h`). See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and [docs/UT61EPLUS_D09A_HANDOVER.md](docs/UT61EPLUS_D09A_HANDOVER.md). Hosted: `?topic=multimeter-wifi-dkholin-ut61eplus-4e8a1c`.

**Public relay (experimental, best-effort):** the firmware also POSTs the latest state to an ntfy.sh topic for the hosted GitHub Pages dashboard. It runs in a separate background task, may be rate-limited (ntfy's free daily quota is shared per IP) and fails silently without affecting the local dashboard, which is the supported path.

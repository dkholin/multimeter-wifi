# Architecture

The meter's LCD SEG/COM waveforms are passively sampled through the CD74HC4067 by the ESP32-C6. The acquisition code derives a stable 60-cell logical state (15 SEG × 4 COM); a state is published only after three matching frames.

The numeric decoder converts the four digit fields and decimal points into a display string. The semantic decoder reads only evidence-backed annunciator cells and produces a normalized state: display/value plus known prefix, mode, and flags. Unknown semantics remain `null` rather than guessed.

The firmware embeds a small HTTP page and an ESP-IDF WebSocket endpoint. `/` serves the dashboard; `/ws` sends normalized states. Raw SEG/COM data is not sent to the browser.

## UNI-T UT61E+ (second meter)

`optical UART poll/parser (firmware/ut61eplus_reader) -> normalized JSON -> Wi-Fi / /ws / ntfy relay -> docs/index.html`

Acquisition code is separate per meter; the state contract and dashboard are shared. `docs/index.html` is served by GitHub Pages (relay, `?topic=`) and embedded into the UNI-T firmware at build time (`embed_page.py`, local `/ws`). Contract: `display`, `value` (display number, in `prefix`+`unit`), `prefix`, `unit`, `manufacturer`, `model`, flags where `null`/absent = unsupported (tag hidden) and `false` = supported but inactive, optional `function`, `mode_raw`, `range_idx`, `battery_mv`, `battery_level` (0-5; icon hidden if absent). Each meter uses its own relay topic and mDNS name (`meter.local` = Crenova, `unit-meter.local` = UNI-T).

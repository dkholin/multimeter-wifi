# Architecture

The meter's LCD SEG/COM waveforms are passively sampled through the CD74HC4067 by the ESP32-C6. The acquisition code derives a stable 60-cell logical state (15 SEG × 4 COM); a state is published only after three matching frames.

The numeric decoder converts the four digit fields and decimal points into a display string. The semantic decoder reads only evidence-backed annunciator cells and produces a normalized state: display/value plus known prefix, mode, and flags. Unknown semantics remain `null` rather than guessed.

The firmware embeds a small HTTP page and an ESP-IDF WebSocket endpoint. `/` serves the dashboard; `/ws` sends normalized states. Raw SEG/COM data is not sent to the browser.

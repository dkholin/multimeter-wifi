# UNI-T UT61E+ / D-09A / XIAO ESP32-C6 handover

## 1. Goal

Reuse the proven Crenova Wi-Fi/dashboard architecture for a UNI-T UT61E+.
Prefer the UNI-T optical/UART interface rather than LCD tapping.

## 2. External protocol findings (verified)

- The UT61E+ uses a polled request/response model; it does not continuously
  broadcast measurements on this cable interface.
- UART settings are 9600 baud, 8 data bits, no parity, 1 stop bit, and no
  flow control (9600 8N1).
- The verified `GetMeasurement` request is:

  ```text
  AB CD 03 5E 01 D9
  ```

- `0x5E` is `GetMeasurement`.
- The checksum is the 16-bit sum of all preceding frame bytes, sent
  big-endian. For this request, `0xAB + 0xCD + 0x03 + 0x5E = 0x01D9`.

Sources: [dmm-tools command implementation](https://github.com/antoinecellerier/dmm-tools/blob/main/crates/dmm-lib/src/protocol/ut61eplus/command.rs) and its [UT61E+ reverse-engineered protocol document](https://github.com/antoinecellerier/dmm-tools/blob/main/docs/research/ut61eplus/reverse-engineered-protocol.md). These sources derive the command framing from the UNI-T vendor software.

## 3. D-09A hardware identification (verified)

- U2 is physically marked `CH9329`.
- Package orientation and pin numbering were established from the package
  marker before tracing. U2 pin 7 is TXD and U2 pin 8 is RXD.
- macOS USB enumeration through the Apple adapter did not produce a visible
  D-09A device. That path was intentionally abandoned in favor of direct
  UART/optical work.

## 4. TX path tracing (verified)

- U2 pin 7 idles at approximately 4.8 V.
- R8 is marked `472` and measured approximately 4.7 kOhm.
- Proven path:

  ```text
  CH9329 pin 7 -> R8 -> Q4 -> right-hand IR LED
  ```

- The CH9329 side of R8 was lifted to isolate CH9329 TX from the optical
  driver.
- After lifting, resistance from U2 pin 7 to the Q4/R8 side was approximately
  5 MOhm, confirming isolation.
- The Q4/R8 side floated around 3.5 V when powered.
- Pulling the Q4/R8 side low drives the IR LED stage.
- The right-hand IR LED switched pad measured about 4.4 V idle and about
  3.88 V with the driver input pulled low.
- During XIAO slow-GPIO validation, the live Crenova Wi-Fi meter showed
  repeatable switching around approximately 3.84 V to 4.01 V.

## 5. XIAO TX setup (verified)

- The XIAO ESP32-C6 is on port 101.
- D6/GPIO16 is used as TX and is connected to the Q4/R8 side of R8.
- XIAO GND and D-09A GND are common.
- The slow-toggle transmitter validation was accepted as PASS.
- The later TX-only firmware sent `AB CD 03 5E 01 D9` once per second at
  9600 8N1.

## 6. RX path tracing (verified)

Measured resistance from U2 pin 8:

- top of R6 to pin 8: 0 Ohm;
- bottom of R6 to pin 8: approximately 0.34 kOhm.

Therefore R6 is the RX-side series element, approximately 360 Ohm. The bottom
of R6 idles at approximately 4.37 V.

## 7. RX divider and wiring (verified)

The raw RX node is approximately 4.37 V, so it was not connected directly to
the XIAO. The divider is:

```text
D-09A bottom of R6 -> 10 kOhm -> XIAO D7/GPIO17 RX
XIAO D7/GPIO17 RX -> 20 kOhm -> GND
```

The XIAO RX node measured approximately 2.9 V idle.

Hardware wiring at pause:

```text
XIAO GND              -> D-09A GND
XIAO D6/GPIO16 TX     -> Q4/R8 side
CH9329 side of R8     remains lifted
D-09A bottom of R6    -> 10 kOhm -> D7/GPIO17 RX
D7/GPIO17 RX          -> 20 kOhm -> GND
```

## 8. RX firmware attempt

The intended minimal meter-link firmware uses UART1 with GPIO16 TX and GPIO17
RX at 9600 8N1. Its intended behavior is to send one poll per second and log
raw received bytes with timestamps to USB Serial/JTAG. It does not decode any
received data.

No RX result was obtained because flashing became unstable.

## 9. Flashing failure and current C6 state (verified)

- Prior TX-only firmware flashed successfully.
- The RX attempt introduced temporary build/upload changes; they were then
  reverted.
- Restored configuration:
  - `upload_speed = 460800`
  - `board_upload.flash_size = 2MB`
  - temporary `sdkconfig.defaults` removed
  - pre-existing `sdkconfig.xiao_esp32c6` left unchanged
- The minimal UART1 firmware builds successfully.
- Normal upload fails with:

  ```text
  Invalid head of packet (0x01): Possible serial noise or corruption.
  ```

- Port 101 still identifies the expected C6 MAC:

  ```text
  58:E6:C5:13:6E:34
  ```

- The board currently enters ESP-ROM download mode:

  ```text
  boot:0x16 (DOWNLOAD...)
  waiting for download
  ```

- No valid RX capture exists yet.

## 10. Current stop point

- The hardware TX path is proven.
- The poll frame is proven.
- The RX physical node and level divider are established.
- RX firmware logic is prepared.
- Work is blocked only on recovering stable flashing/boot of the C6.
- Do not re-trace D-09A hardware unless new evidence contradicts these
  measurements.

## 11. Next-agent guidance

1. Recover the XIAO to a bootable state using the last known-good flashing
   path.
2. Avoid changing flash geometry, partition layout, console routing, or SDK
   configuration unless first justified.
3. Once stable, flash the minimal UART1 TX/RX firmware.
4. Capture raw RX bytes after each poll.
5. Do not decode until repeatable raw responses are proven.

## Assumptions and unverified items

- No UT61E+ response frame has been captured on D7; RX electrical signaling,
  byte framing, and response content remain unverified.
- The divider is verified by its component values and idle measurement, but its
  behavior during received UART transitions has not yet been observed.
- No conclusion should be drawn about meter-side optical alignment or response
  decoding until a repeatable raw RX capture exists.

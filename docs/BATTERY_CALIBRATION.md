# UT61E+ battery: charging behavior and ADC calibration

Investigated 2026-09-27. Hardware: LiPo -> XIAO ESP32-C6 BAT pads (also charges via USB-C);
LiPo -> boost converter VIN -> 5V -> D-09A. BAT+ -> 200k -> D2/GPIO2 -> 200k -> GND for ADC sensing.

## Charger IC (from the official Seeed XIAO ESP32-C6 schematic, `XIAO-ESP32-C6_v1.0_SCH_PDF_24028.pdf`)

- U3 = **SGM40567-4.2XG/TR** (SG Micro), a linear single-cell Li-ion CC/CV charger, float
  voltage variant 4.20V (4.257V if its F pin is strapped to BAT rather than GND, which the
  schematic suggests -- and which matches the 4.250V disconnected/rested reading closely).
- Charge current set by R_IREF = 200k on the IREF pin; Seeed's schematic prints
  `ICharge = 24000/200k = 120mA`, matching the datasheet formula for ICHG <= 400mA.
  Datasheet current-distribution data implies realistic spread is roughly +-10% of programmed.
- From the datasheet ("Detailed Description", Fig. 4/5): pre-charge at 7.5%xICHG below 60% of
  float voltage, then constant-current at ICHG, then constant-voltage once >=98.5% of float.
  Terminates ("full-charge") when current drops below 6.5%xICHG (~8mA) or after a 44-minute CV
  timeout, then drops into a voltage fold-back hold (~4% below float) rather than sitting at
  float indefinitely -- this is deliberate battery-longevity behavior, not a fault. Auto-recharges
  if the held voltage sags >1.5%. CHG LED: blinks (1/8 duty, 1280ms period) while charging, solid
  for 51.2s once full, then off.
- Block diagram (sheet 2/5): VBAT reaches the board's own 5V->3.3V buck (SGM6029C) only through a
  PFET gated by a comparator sensing VBUS; VBUS reaches the same buck via a Schottky diode. So the
  XIAO's own Wi-Fi/system load is intended to run from USB, not the battery, while USB is present.

## The wrinkle: boost + D-09A bypass that mux entirely

The boost converter is wired directly to the raw BAT+/BAT- pads (per our own wiring notes), not
through the XIAO's USB/battery power path. So even while charging over USB-C, the boost + D-09A
keep drawing continuously from the battery, competing with the charger's ~120mA.

**Live observation (2026-09-27, USB plugged in, meter running normally):** `battery_mv` dropped
from ~4095mV to ~3990mV within seconds of plugging in USB, then held flat at 3990-3992mV for the
next 90+ seconds. The charge LED was **blinking** (actively charging, not done) the whole time.
This is consistent with the boost+D-09A load roughly offsetting the ~120mA charge current: the
charger keeps regulating (hence the blink) but net current into the battery is near zero, so the
terminal voltage sits in a rough equilibrium rather than climbing toward the ~4.2-4.26V float
target. No invasive current-shunt measurement was needed to establish this -- the LED + voltage
trend together are sufficient evidence.

This explains the previously-reported "~3.9V ceiling" without needing to assume a defective
charger: it's a combination of (a) the pre-1.0245-correction ADC under-reading (mapping a true
~3.99V to a displayed ~3.9V), and (b) this load/charge-current equilibrium under the boost+D-09A
draw. A battery charged with the D-09A *unpowered* (RJ/USB disconnected from the boost, or boost
physically unplugged) would be expected to climb closer to true float voltage; that was not tested
this session since it requires opening the boost wiring.

## Power budget (rough ranges, no invasive current measurement)

Not independently current-metered this session; ranges below are order-of-magnitude sanity checks,
not a substitute for the live LED/voltage evidence above:

- XIAO ESP32-C6 running Wi-Fi station + small HTTP/WS server + 1Hz UART polling: roughly 30-70mA
  average at 3.3V (light periodic traffic, occasional TX bursts).
- D-09A (CH9329 bridge + optical driver/LED): roughly 20-50mA at 5V, unconfirmed for this specific
  board -- CH9329 datasheet was not fetched this session.
- Boost converter efficiency: unknown part; assumed 75-85% typical for a small single-cell-LiPo-to-5V
  boost at these load levels.
- Rough battery-side draw while running on battery alone (no USB): C6 load (through the buck, when
  VBAT feeds it) plus boost+D-09A load (through the boost) sums to very roughly 90-150mA, consistent
  in order of magnitude with the charger's 120mA setting being a close match rather than a large
  margin -- which is exactly why the observed near-equilibrium during charging is plausible.

## ADC calibration (paired DMM measurement, 2026-09-27)

Measured DMM directly across boost VIN+/VIN- (same node as BAT, since the boost is wired straight
to the battery pads) with: battery connected, USB unplugged, firmware running normally.

- DMM: **4095mV**
- Firmware `battery_mv` at the same moment: **4216mV** (old gain 1.0245)
- Raw 2x-pin average (pre-gain): 4216 / 1.0245 = 4115mV
- New gain: 4095 / 4115 = **0.9951** (`BAT_CAL_PPM = 995100` in `firmware/ut61eplus_reader/src/battery.c`)

Re-flashed and re-verified live: firmware now reads 4094-4095mV against the same 4095mV DMM
reading -- match confirmed on real hardware, not just computed.

## Battery icon thresholds

Lower 4 thresholds rescaled by the calibration ratio (x0.9713) to preserve their original
real-world meaning under the corrected gain (not a second, independent retune of the same error).
Top threshold set from the new paired evidence instead: a battery charged to 4.250V (rested) settles
to ~4095mV while running this system under load (boost+D-09A sag via ESR, not depletion), so the old
4100mV top threshold was never reachable in practice.

| Level | Old (mV) | New (mV) |
|---|---|---|
| 1 | 3400 | 3300 |
| 2 | 3650 | 3550 |
| 3 | 3800 | 3690 |
| 4 | 3950 | 3840 |
| 5 | 4100 | 4050 |

Still six coarse levels (0-5), same hysteresis (40mV), no percentage -- unchanged from before.

# Stage 3 validation (LCD vs decoder vs browser)

Each case compared the physical LCD (as read by the operator), the normalized state from `/ws`, and the browser DOM. Low-voltage bench only.

| # | Case | LCD | Decoder / browser | Result |
|---|------|-----|-------------------|--------|
| 1 | +DC (AA-type cell) | 3.900 V | 3.900 V, DC, AUTO | PASS |
| 2 | -DC (probes reversed) | -3.901 V | -3.901 V, DC, AUTO | PASS |
| 3 | DC mV zero | 000.0 mV | 000.0 mV, DC, AUTO | PASS |
| 4 | Resistance, shorted | 000.0 ohm | 000.0 Ω, AUTO | PASS |
| 5 | Resistance, open | OL (M ohm) | OL, MΩ, AUTO, ol=true | PASS |
| 6 | Capacitance | 0.010 / 0.003 nF | 0.003 nF, AUTO | PASS |
| 7 | Diode, open | OL | OL, V, diode=true | PASS |
| 7b | Diode, probes shorted (final firmware) | 0.000 V | 0.000 V, diode=true | PASS |
| 8 | Continuity, open | OL | OL, Ω, continuity=true | PASS |
| 9 | AUTO -> manual range | AUTO off | auto=false | PASS |
| 10 | HOLD on / off | HOLD | hold=true / false | PASS |
| 11 | MAX (DC mV) | MAX 001.2 | max=true, 001.2 mV | PASS |

Observations: after a dial turn the published state can lag by several seconds while capture re-syncs; it always converged. MIN was not exercised (cell 58 was already proven). Diode with a real forward-biased junction was not tested. One early diode capture (operator read 0.639 V, decoded 2.737 V) did not match; it was not reproduced on the final firmware (steady OL and 0.000 V both matched).

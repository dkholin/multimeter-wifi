# Mapping status

The canonical machine-readable maps are [decoder/numeric_map.json](../decoder/numeric_map.json) and [decoder/semantic_map.json](../decoder/semantic_map.json). No other semantic map is authoritative in this repository. LCD cells use `0` for lit and `1` for unlit.

## Proven

- Four digit positions and all three decimal-point positions are validated for positive numeric strings. The leading-digit top/bottom geometry remains an explicitly handled interchangeable pair.
- `AUTO` (52), `HOLD` (55), `DC` (49), `AC` (51), `MAX` (59), and `MIN` (58).
- Prefix indications: mV `m` (11), k (10), M (13), nF `n` (14), mF `m` (15); Hz (1); percent (9).
- Units and modes (Stage 3 controlled captures, [evidence](../evidence/stage3_semantic_captures.json)): V (3), ohm (6), F (7), diode (53), continuity (54). Each was checked against at least two other modes.
- OL is display text, not an annunciator: blank, `0`, `L`, blank (decimal point position varies). Decoded from the digit cells.
- Minus sign: `SEG12xCOM2` (50), lit=`0`. It was confirmed across negative → positive → negative held readings and is implemented in the dashboard firmware.

## Unresolved / untested

- Micro prefix and any other capacitance prefix cells; the prefix is `null` when no known prefix cell is lit.
- `SEG12xCOM0` (48) is lit in every captured state; meaning not isolated.
- Current units/prefixes, APO, battery, NCV, and Live.
- Diode with a real forward-biased junction; MIN annunciator was not re-exercised in Stage 3.

The compact files in [evidence/](../evidence/) summarize the controlled validation. Large raw capture histories are intentionally excluded.

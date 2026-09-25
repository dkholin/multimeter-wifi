# Mapping status

The canonical machine-readable maps are [decoder/numeric_map.json](../decoder/numeric_map.json) and [decoder/semantic_map.json](../decoder/semantic_map.json). No other semantic map is authoritative in this repository. LCD cells use `0` for lit and `1` for unlit.

## Proven

- Four digit positions and all three decimal-point positions are validated for positive numeric strings. The leading-digit top/bottom geometry remains an explicitly handled interchangeable pair.
- `AUTO` (52), `HOLD` (55), `DC` (49), `AC` (51), `MAX` (59), and `MIN` (58).
- Prefix indications: mV `m` (11), k (10), M (13), nF `n` (14), mF `m` (15); Hz (1); percent (9).
- Minus sign: `SEG12xCOM2` (50), lit=`0`. It was confirmed across negative → positive → negative held readings and is implemented in the dashboard firmware.

## Unresolved / untested

- Individually identifying V versus ohm, F, diode, continuity, and OL.
- Current units/prefixes, APO, battery, NCV, and Live.
- Physical coverage of every digit at every position; signs and OL are outside the original numeric-decoder validation scope.

The compact files in [evidence/](../evidence/) summarize the controlled validation. Large raw capture histories are intentionally excluded.

# Stage 2B.5 live numeric validation

| Reading tested | Physical LCD label | Decoded output | Exact match / pass status |
|---|---|---|---|
| 1 | `01.85` | `01.85` | PASS — 73 valid, 0 invalid, one state |
| 2 | `01.65` | `01.65` | PASS — 73 valid, 0 invalid, one state |
| 3 | `001.9` | `001.9` | PASS — 73 valid, 0 invalid, one state |
| 4 | `2.286` | `2.286` | PASS — 73 valid, 0 invalid, one state |
| 5 | `2.467` | `2.467` | PASS — 72 valid, 0 invalid, one state |
| 6, returned reading 1 | `01.85` | `01.85` | PASS — 71 valid, 0 invalid, one state; exact state-ID reproduction: `d2e220bd0aaf` |

Anomaly: none. Live acquisition remained stable; all six captures had one logical state and no invalid records.

Live wrapper: `live_decode.py`.

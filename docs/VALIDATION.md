# Validation

The positive numeric decoder passed three unseen held-display cases: `001.8`, `1.855`, and `01.85`, with 48–49 identical valid frames per capture. See [evidence/numeric_live_validation.md](../evidence/numeric_live_validation.md).

Stage 3 browser validation displayed a held physical `-004.5 mV`, DC, AUTO, HOLD reading after the minus mapping was promoted. The reference state and candidate comparison are retained in `evidence/`.

The firmware's serial calibration must observe four active COM bands from the connected meter before live states are published. Re-run the local compile command in the README after changing firmware.

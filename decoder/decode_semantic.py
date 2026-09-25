#!/usr/bin/env python3
"""Decode an LCD matrix using the canonical numeric and semantic maps."""
import argparse
import json
from pathlib import Path

from decode_numeric import decode


def active(matrix, index):
    return matrix[index] == "0"


def decode_state(matrix, numeric_map, semantic_map):
    if len(matrix) != 60 or set(matrix) - {"0", "1"}:
        raise ValueError("Expected exactly 60 binary cells")
    cells = semantic_map["confirmed_cells"]
    by_semantic = {item["semantic"]: item["index"] for item in cells.values()}
    result = {name: active(matrix, by_semantic[name]) for name in ("auto", "hold", "max", "min", "minus")}
    result["mode"] = "AC" if active(matrix, by_semantic["AC"]) else "DC" if active(matrix, by_semantic["DC"]) else None
    result["indicators"] = [name for name in ("Hz", "percent", "k_prefix", "M_prefix", "m_prefix_voltage", "n_prefix_capacitance", "m_prefix_capacitance") if active(matrix, by_semantic[name])]
    try:
        display = decode(matrix, numeric_map)
        result["display"] = ("-" if result["minus"] else "") + display
        result["value"] = float(result["display"])
    except ValueError:
        result["display"] = None
        result["value"] = None
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("state", type=Path, help="JSON state containing matrix60")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    matrix = json.loads(args.state.read_text())["matrix60"]
    print(json.dumps(decode_state(matrix, json.loads((root / "numeric_map.json").read_text()), json.loads((root / "semantic_map.json").read_text())), indent=2))

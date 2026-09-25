#!/usr/bin/env python3
"""Unit-independent positive numeric decoder; unresolved sign/OL unsupported."""
import argparse
import json
from pathlib import Path
from solve_numeric import DIGITS

def decode(matrix, mapping):
    if len(matrix) != 60 or set(matrix) - set('01'):
        raise ValueError('Expected exactly 60 binary cells')
    output = ''
    for pos in range(4):
        options = [None] if pos else [False, True]
        matches = set()
        for swap in options:
            lit = set()
            for seg in 'abcdefg':
                cells = mapping['segments'][f'{pos}.{seg}']
                if len(cells) == 1:
                    cell = cells[0]
                elif pos == 0 and seg in 'ad' and len(cells) == 2:
                    cell = cells[(seg == 'd') ^ bool(swap)]
                else:
                    raise ValueError('Unresolved segment assignment')
                if matrix[cell] == '0':
                    lit.add(seg)
            matches.update(d for d, pattern in DIGITS.items() if lit == set(pattern))
        if len(matches) != 1:
            raise ValueError(f'Unrecognized or ambiguous digit at position {pos}: {sorted(matches)}')
        output += next(iter(matches))
    points = [int(pos) for pos, cells in mapping['decimal_after_zero_based_position'].items()
              if len(cells) == 1 and matrix[cells[0]] == '0']
    if len(points) > 1:
        raise ValueError('Multiple decimal points lit')
    if points:
        n = points[0] + 1
        output = output[:n] + '.' + output[n:]
    return output

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('state', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    print(decode(json.loads(args.state.read_text())['matrix60'],
                 json.loads((root / 'numeric_map.json').read_text())))

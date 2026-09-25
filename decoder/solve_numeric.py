#!/usr/bin/env python3
"""Report candidate cells from construction samples; never promote by fit alone."""
import json
from pathlib import Path

DIGITS = dict(zip('0123456789', ('abcdef', 'bc', 'abdeg', 'abcdg',
                                'bcfg', 'acdfg', 'acdefg', 'abc',
                                'abcdefg', 'abcdfg')))

def solve(root):
    dataset = json.loads((root / 'dataset.json').read_text())
    rows = [r for r in dataset['samples'] if r['role'] == 'construction']
    matrices = [json.loads((root / r['state_file']).read_text())['matrix60'] for r in rows]
    strings = [r['physical_numeric_string'].replace('.', '').lstrip('-') for r in rows]
    assert all(len(m) == 60 and set(m) <= set('01') for m in matrices)
    assert all(len(s) == 4 and s.isdigit() for s in strings)
    candidates = {}
    for position in range(4):
        for segment in 'abcdefg':
            expected = ''.join('0' if segment in DIGITS[s[position]] else '1' for s in strings)
            candidates[f'{position}.{segment}'] = [
                f'SEG{i//4}xCOM{i%4}' for i in range(60)
                if i not in (52, 55) and ''.join(m[i] for m in matrices) == expected
            ]
    result = {'status': 'candidates_only_not_confirmed',
              'geometry': 'a top; b upper-right; c lower-right; d bottom; e lower-left; f upper-left; g middle',
              'construction_labels': [r['label'] for r in rows],
              'segment_candidates': candidates}
    (root / 'candidate_map.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'samples': len(rows), 'unique_candidates': sum(len(v) == 1 for v in candidates.values()),
                      'missing_candidates': [k for k,v in candidates.items() if not v],
                      'candidate_counts': {k:len(v) for k,v in candidates.items()}}))

if __name__ == '__main__':
    solve(Path(__file__).resolve().parent)

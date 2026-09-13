#!/usr/bin/env python3
"""Exercise six native-relay outcomes and repeat the seed to check determinism."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    """Retain each run's logs; fail if any case or the repeatability check fails."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', required=True)
    args = parser.parse_args()
    cases = ['normal', 'wrong-subject', 'expired', 'lost-receipt', 'conflict', 'deadline', 'normal']
    results = []
    for number, case in enumerate(cases):
        run_id = f'{args.prefix}-{number}-{case}'
        result = subprocess.run([sys.executable, str(ROOT / 'environment/smoke.py'),
            '--program', 'scrap-relay-lifecycle', '--case', case, '--run-id', run_id])
        if result.returncode:
            raise SystemExit(f'{run_id} failed; inspect its console.log and manifest.json')
        log = (ROOT / 'runs' / run_id / 'console.log').read_text()
        lines = [line.removeprefix('RESULT ') for line in log.splitlines()
                 if line.startswith('RESULT ')]
        if len(lines) != 1:
            raise SystemExit(f'{run_id}: expected one terminal outcome')
        outcome = json.loads(lines[0])
        if outcome['case'] != case or outcome['passed'] is not True:
            raise SystemExit(f'{run_id}: unexpected outcome: {outcome}')
        results.append(outcome)
        print(json.dumps(outcome), flush=True)
    if results[0] != results[-1]:
        raise SystemExit('Same-seed normal case was not reproducible')
    print('PASS: six lifecycle cases and exact same-seed outcome repeatability')


if __name__ == '__main__':
    main()

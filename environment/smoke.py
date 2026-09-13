#!/usr/bin/env python3
"""Run a pinned SNS-3 example in a unique directory and retain failure evidence."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

from bootstrap import ROOT, checkout


def digest(path):
    """Hash exact input bytes, not a reserialized approximation."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    """Record inputs and process status; success is connectivity smoke evidence only."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scenario', default='constellation-eutelsat-geo-2-sats-isls')
    parser.add_argument('--program', choices=['sat-constellation-example',
                        'scrap-relay-lifecycle'], default='sat-constellation-example')
    parser.add_argument('--case', choices=['normal', 'wrong-subject', 'expired',
                        'lost-receipt', 'conflict', 'deadline'], default='normal')
    parser.add_argument('--run-id', required=True)
    parser.add_argument('--seed', type=int, default=1)
    parser.add_argument('--run', type=int, default=1)
    args = parser.parse_args()
    if os.name != 'posix':
        parser.error('Run in Linux or WSL2.')
    for value in (args.run_id, args.scenario):
        if not re.fullmatch(r'[a-zA-Z0-9][a-zA-Z0-9_-]{0,100}', value):
            parser.error('Scenario and run ID must be simple names, not paths.')
    if not 1 <= args.seed < 2**32 or not 1 <= args.run < 2**64:
        parser.error('Seed must be a positive uint32; run must be a positive uint64.')
    lock = json.loads((ROOT / 'deps.lock').read_text())
    for item in lock['sources'].values():
        path = ROOT / item['path']
        if not (path / '.git').exists():
            parser.error('Dependencies missing; run bootstrap first.')
        checkout(path, item)
    scenario = ROOT / '.deps/ns-3/contrib/satellite/data/scenarios' / args.scenario
    if not scenario.is_dir():
        parser.error('Scenario not found in pinned satellite data.')
    build = ROOT / '.deps/ns-3/build'
    binaries = list(build.rglob('ns*-' + args.program + '-default'))
    if len(binaries) != 1:
        parser.error('Build the selected program with the default profile first.')
    binary = binaries[0]
    output = ROOT / 'runs' / args.run_id
    output.mkdir(parents=True, exist_ok=False)
    # SNS-3 locates data during static initialization, before command-line parsing.
    data_parent = output / 'contrib/satellite'
    data_parent.mkdir(parents=True)
    (data_parent / 'data').symlink_to(ROOT / '.deps/ns-3/contrib/satellite/data')
    stats = output / 'statistics'
    stats.mkdir()
    program = shlex.join([args.program,
        '--scenarioFolder=' + args.scenario, '--OutputPath=' + str(stats),
        '--RngSeed=' + str(args.seed), '--RngRun=' + str(args.run)] +
        (['--case=' + args.case] if args.program == 'scrap-relay-lifecycle' else []))
    command = ['bash', str(ROOT / 'environment/ns3.sh'), 'run', program,
               '--no-build', '--cwd=' + str(output)]
    git = lambda *items: subprocess.check_output(['git', *items], cwd=ROOT, text=True).strip()
    inputs = {str(p.relative_to(scenario)): digest(p)
              for p in sorted(scenario.rglob('*')) if p.is_file()}
    sources = git('ls-files', '-c', '-o', '--exclude-standard').splitlines()
    manifest = {'schema_version': 1, 'kind': args.program, 'case': args.case,
        'started_utc': datetime.now(timezone.utc).isoformat(),
        'git_commit': git('rev-parse', 'HEAD'), 'git_status': git('status', '--porcelain'),
        'source_sha256': {p: digest(ROOT / p) for p in sources if (ROOT / p).is_file()},
        'dependencies': lock, 'scenario_sha256': inputs,
        'binary_sha256': digest(binary), 'binary': str(binary),
        'cmake_cache_sha256': digest(ROOT / '.deps/ns-3/cmake-cache/CMakeCache.txt'),
        'seed': args.seed, 'run': args.run, 'command': command,
        'status': 'running', 'python': sys.version,
        'compiler': subprocess.check_output(['c++', '--version'], text=True).splitlines()[0]}
    path = output / 'manifest.json'
    def save():
        """Write atomically so interruption leaves a readable manifest."""
        temporary = path.with_suffix('.tmp')
        temporary.write_text(json.dumps(manifest, indent=2) + '\n')
        temporary.replace(path)
    save()
    try:
        with (output / 'console.log').open('w') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        manifest.update(returncode=result.returncode,
                        status='completed' if result.returncode == 0 else 'failed')
    except BaseException as error:
        manifest.update(status='interrupted', error=repr(error))
        raise
    finally:
        manifest['finished_utc'] = datetime.now(timezone.utc).isoformat()
        save()
    print(path)
    return result.returncode


if __name__ == '__main__':
    sys.exit(main())

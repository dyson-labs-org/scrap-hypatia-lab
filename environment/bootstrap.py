#!/usr/bin/env python3
"""Fetch locked sources and configure SNS-3 without changing upstream code."""
import json
import os
from pathlib import Path
import subprocess
import venv

ROOT = Path(__file__).resolve().parents[1]


def run(*args, cwd=None, capture=False):
    """Run a checked command, optionally returning its standard output."""
    return subprocess.run(args, cwd=cwd, check=True, text=True,
                          stdout=subprocess.PIPE if capture else None).stdout


def checkout(path, item):
    """Fetch an exact revision; resume incomplete fetches, refuse changed sources."""
    path.mkdir(parents=True, exist_ok=True)
    if not (path / '.git').exists():
        if any(path.iterdir()):
            raise RuntimeError(f'{path}: refusing to initialize a nonempty directory')
        run('git', 'init', '-q', str(path))
        run('git', 'remote', 'add', 'origin', item['url'], cwd=path)
    origin = run('git', 'remote', 'get-url', 'origin', cwd=path, capture=True).strip()
    if origin != item['url']:
        raise RuntimeError(f'{path}: origin does not match lock')
    head = subprocess.run(['git', 'rev-parse', '--verify', 'HEAD'], cwd=path,
                          text=True, capture_output=True)
    if head.returncode:
        run('git', 'fetch', '--depth', '1', '--filter=blob:none',
            'origin', item['commit'], cwd=path)
        if item.get('sparse'):
            run('git', 'sparse-checkout', 'init', '--cone', cwd=path)
            run('git', 'sparse-checkout', 'set', *item['sparse'], cwd=path)
        run('git', 'checkout', '--detach', 'FETCH_HEAD', cwd=path)
    actual = run('git', 'rev-parse', 'HEAD', cwd=path, capture=True).strip()
    if actual != item['commit']:
        raise RuntimeError(f'{path}: expected {item["commit"]}, found {actual}')
    run('git', 'diff', '--exit-code', '--ignore-submodules=all', 'HEAD', cwd=path)
    for entry in item.get('sparse', []):
        if not (path / entry).is_dir():
            raise RuntimeError(f'{path}: required data directory missing: {entry}')


def main():
    """Prepare sources, isolated build tools, and the ns-3 configuration."""
    if os.name != 'posix':
        raise SystemExit('Run bootstrap in Linux or WSL2.')
    lock = json.loads((ROOT / 'deps.lock').read_text())
    tools = ROOT / '.deps/tools'
    if not tools.exists():
        venv.create(tools, with_pip=True)
    run(str(tools / 'bin/python'), '-m', 'pip', 'install', '-r',
        str(ROOT / 'environment/requirements.txt'))
    os.environ['PATH'] = str(tools / 'bin') + os.pathsep + os.environ['PATH']
    for item in lock['sources'].values():
        path = (ROOT / item['path']).resolve()
        if not path.is_relative_to((ROOT / '.deps').resolve()):
            raise RuntimeError('Dependency path must remain inside .deps')
        checkout(path, item)
    ns3 = ROOT / '.deps/ns-3'
    for name in ('relay-lifecycle', 'onboard-downlink'):
        source = ROOT / f'src/scrap/{name}.cc'
        link = ns3 / f'scratch/scrap-{name}.cc'
        if not link.exists():
            link.symlink_to(source)
        if link.resolve() != source:
            raise RuntimeError('Unexpected SCRAP integration source path')
    run(str(ns3 / 'ns3'), 'configure', '-G', 'Ninja', '--build-profile=default',
        '--enable-asserts', '--enable-tests', '--enable-examples',
        '--disable-python-bindings', '--disable-werror',
        '--enable-modules=satellite,traffic,magister-stats', cwd=ns3)


if __name__ == '__main__':
    main()

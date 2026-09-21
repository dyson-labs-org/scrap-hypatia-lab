#!/usr/bin/env python3
"""Validate onboard downlink traces independently of the application's pass flag."""
import argparse
import json
import math
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
LEO = 'constellation-leo-2-satellites'
GEO = 'constellation-eutelsat-geo-2-sats-isls'


def validate(result, events, *, case, payload_bytes):
    """Use fixed fixture physics and observed timestamps, not RESULT.passed."""
    if result['case'] != case or result['payload_bytes'] != payload_bytes:
        raise ValueError('Run did not execute the requested fixture')
    if result['sent'] != 1 or result['source_node'] == result['receiver_node']:
        raise ValueError('Expected one packet between distinct satellite and ground nodes')
    enqueues = [e for e in events if e['event'] == 'onboard-enqueue']
    frames = [e for e in events if e['event'] == 'feeder-frame-tx']
    receives = [e for e in events if e['event'] == 'ground-application-rx']
    if len(enqueues) != 1 or enqueues[0]['device_type'] != 'ns3::SatOrbiterNetDeviceDvb':
        raise ValueError('Missing onboard application/device evidence')
    if enqueues[0]['source_node'] != result['source_node']:
        raise ValueError('Onboard node identity mismatch')
    if case == 'tx-disabled':
        if frames or receives or result['received'] != 0 or result['frames'] != 0:
            raise ValueError('Data bypassed the disabled native transmitter')
        return
    if len(frames) != 1 or len(receives) != 1:
        raise ValueError('Expected exactly one native data frame and one ground delivery')
    if result['received'] != 1 or result['frames'] != 1 or result['payload_valid'] is not True:
        raise ValueError('Ground payload was missing, duplicated, or corrupt')
    frame, receive = frames[0], receives[0]
    if frame['source_node'] != result['source_node'] or frame['device_index'] != result['source_device']:
        raise ValueError('Native egress differs from onboard source')
    if receive['receiver_node'] != result['receiver_node'] or receive['device_index'] != result['receiver_device']:
        raise ValueError('Ground receiver identity mismatch')
    if receive['payload_bytes'] != payload_bytes:
        raise ValueError('Received size differs from generated payload')

    # Fixed DVB-S2 profile: 64800 coded bits / 2 bits per QPSK symbol,
    # a 90-symbol physical header, and 22 pilot blocks of 36 symbols.
    symbol_rate = 125_000_000 / 1.2
    frame_seconds = (64800 / 2 + 90 + 22 * 36) / symbol_rate
    air_seconds = frame_seconds - 0.000001  # Explicit native MAC guard time.
    tolerance = 0.000002
    if (result['bandwidth_hz'] != 125_000_000 or result['roll_off'] != 0.2 or
            not math.isclose(result['guard_s'], 0.000001, rel_tol=0, abs_tol=1e-15)):
        raise ValueError('Timing profile changed; independent expectation needs review')
    for value in (frame['time_s'], frame['frame_duration_s'], frame['distance_m'], receive['time_s']):
        if not math.isfinite(value):
            raise ValueError('Nonfinite observation')
    if frame['distance_m'] <= 0:
        raise ValueError('Missing satellite-to-station distance')
    if abs(frame['frame_duration_s'] - frame_seconds) > tolerance:
        raise ValueError('Frame serialization does not match the fixed DVB-S2 profile')
    queued = frame['time_s'] - enqueues[0]['time_s']
    if not 0 <= queued <= 2 * frame_seconds:
        raise ValueError('Uncongested packet spent too long waiting for its native frame')
    expected_receive = frame['time_s'] + air_seconds + frame['distance_m'] / 299_792_458
    if abs(receive['time_s'] - expected_receive) > tolerance:
        raise ValueError(f'Propagation/serialization mismatch: {receive["time_s"]} vs {expected_receive}')
    if result['tx_s'] != frame['time_s'] or result['received_s'] != receive['time_s']:
        raise ValueError('Terminal result disagrees with native/application event timestamps')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', required=True)
    args = parser.parse_args()
    cases = [(LEO, 'normal', 1024, 1.0), (LEO, 'normal', 64, 1.013),
             (LEO, 'normal', 2048, 1.027), (GEO, 'normal', 1024, 1.0),
             (LEO, 'tx-disabled', 1024, 1.0), (LEO, 'normal', 1024, 1.0)]
    evidence = []
    for index, (scenario, case, size, start) in enumerate(cases):
        run_id = f'{args.prefix}-{index}-{case}'
        subprocess.run([sys.executable, str(ROOT / 'environment/smoke.py'),
                        '--program', 'scrap-onboard-downlink', '--scenario', scenario,
                        '--case', case, '--payload-bytes', str(size), '--send-at', str(start),
                        '--run-id', run_id], check=True)
        folder = ROOT / 'runs' / run_id
        lines = (folder / 'console.log').read_text().splitlines()
        terminal = [json.loads(line[7:]) for line in lines if line.startswith('RESULT ')]
        events = [json.loads(line[6:]) for line in lines if line.startswith('EVENT ')]
        if len(terminal) != 1:
            raise ValueError(f'{run_id}: expected one terminal result')
        validate(terminal[0], events, case=case, payload_bytes=size)
        manifest = json.loads((folder / 'manifest.json').read_text())
        if manifest['status'] != 'completed' or manifest['returncode'] != 0:
            raise ValueError(f'{run_id}: incomplete execution')
        evidence.append((terminal[0], events))
        print(json.dumps({'run_id': run_id, 'validated': True, 'result': terminal[0]}), flush=True)
    if evidence[0] != evidence[-1]:
        raise ValueError('Same-seed results or observed event order are not reproducible')
    print('PASS: native onboard LEO/GEO downlink, payload sizes, timing, disabled transmitter, repeatability')


if __name__ == '__main__':
    main()

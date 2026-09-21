"""Independent timing validator must reject plausible but invalid simulator evidence."""
import copy
import importlib.util
from pathlib import Path
import unittest

path = Path(__file__).with_name('run_onboard.py')
spec = importlib.util.spec_from_file_location('run_onboard', path)
onboard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(onboard)


class EvidenceTests(unittest.TestCase):
    def fixture(self):
        # Deliberately external arithmetic for a 300 km link and fixed DVB-S2 frame.
        tx = 1.0001
        frame = 33282 / (125000000 / 1.2)
        rx = tx + frame - 0.000001 + 300000 / 299792458
        result = dict(case='normal', payload_bytes=1024, sent=1, received=1, frames=1,
                      payload_valid=True, source_node=0, source_device=0,
                      receiver_node=2, receiver_device=1, bandwidth_hz=125000000,
                      roll_off=0.2, guard_s=0.000001, tx_s=tx, received_s=rx,
                      passed=True)
        events = [
            dict(event='onboard-enqueue', time_s=1.0, source_node=0,
                 device_type='ns3::SatOrbiterNetDeviceDvb'),
            dict(event='feeder-frame-tx', time_s=tx, source_node=0, device_index=0,
                 distance_m=300000, frame_duration_s=frame),
            dict(event='ground-application-rx', time_s=rx, receiver_node=2,
                 device_index=1, payload_bytes=1024)]
        return result, events

    def test_accepts_independent_valid_fixture(self):
        result, events = self.fixture()
        onboard.validate(result, events, case='normal', payload_bytes=1024)

    def test_accepts_guard_float_roundoff_but_not_a_changed_guard(self):
        result, events = self.fixture()
        result['guard_s'] = 9.9999999999997e-07
        onboard.validate(result, events, case='normal', payload_bytes=1024)
        result['guard_s'] = 0.000002
        with self.assertRaisesRegex(ValueError, 'Timing profile changed'):
            onboard.validate(result, events, case='normal', payload_bytes=1024)

    def test_rejects_instantaneous_delivery_even_if_pass_flag_true(self):
        result, events = self.fixture()
        events[-1]['time_s'] = events[1]['time_s']
        result['received_s'] = events[-1]['time_s']
        with self.assertRaisesRegex(ValueError, 'Propagation/serialization'):
            onboard.validate(result, events, case='normal', payload_bytes=1024)

    def test_rejects_bypassed_native_egress(self):
        result, events = self.fixture()
        events.pop(1)
        with self.assertRaisesRegex(ValueError, 'native data frame'):
            onboard.validate(result, events, case='normal', payload_bytes=1024)

    def test_rejects_duplicate_delivery(self):
        result, events = self.fixture()
        events.append(copy.deepcopy(events[-1]))
        with self.assertRaisesRegex(ValueError, 'one ground delivery'):
            onboard.validate(result, events, case='normal', payload_bytes=1024)

    def test_rejects_wrong_source_device(self):
        result, events = self.fixture()
        events[1]['device_index'] = 7
        with self.assertRaisesRegex(ValueError, 'Native egress'):
            onboard.validate(result, events, case='normal', payload_bytes=1024)

    def test_disabled_transmitter_requires_no_arrival(self):
        result, events = self.fixture()
        result['case'] = 'tx-disabled'
        with self.assertRaisesRegex(ValueError, 'bypassed'):
            onboard.validate(result, events, case='tx-disabled', payload_bytes=1024)

    def test_tolerance_cannot_be_relaxed_by_result(self):
        result, events = self.fixture()
        result['tolerance_s'] = 100
        events[-1]['time_s'] += 0.01
        result['received_s'] += 0.01
        with self.assertRaisesRegex(ValueError, 'Propagation/serialization'):
            onboard.validate(result, events, case='normal', payload_bytes=1024)


if __name__ == '__main__':
    unittest.main()

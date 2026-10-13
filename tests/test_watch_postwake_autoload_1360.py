"""Offline failure-path checks for the read-only autoload watcher."""

import importlib.util
import unittest
from pathlib import Path
from unittest.mock import patch


MODULE = Path(__file__).resolve().parents[1] / "diagnostics" / "watch_postwake_autoload_1360.py"
SPEC = importlib.util.spec_from_file_location("watch_postwake_autoload_1360", MODULE)
watch = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(watch)


class WatcherTests(unittest.TestCase):
    def test_clean_early_window(self):
        before = {"remaining_ms": 4200, "done": 0, "total": 0}
        after = {"remaining_ms": 3600, "done": 0, "total": 6}
        self.assertTrue(watch.early_window(before))
        self.assertTrue(watch.early_uncontaminated(before, after))

    def test_autoload_overlap_invalidates_early(self):
        before = {"remaining_ms": 2900, "done": 0, "total": 6}
        self.assertFalse(watch.early_uncontaminated(
            before, {"remaining_ms": 0, "done": 1, "total": 6}))
        self.assertFalse(watch.early_uncontaminated(
            before, {"remaining_ms": 0, "done": 0, "total": 6}))

    def test_late_or_started_window_rejected(self):
        self.assertFalse(watch.early_window(
            {"remaining_ms": 2499, "done": 0, "total": 6}))
        self.assertFalse(watch.early_window(
            {"remaining_ms": 4500, "done": 1, "total": 6}))
        self.assertFalse(watch.early_window(
            {"remaining_ms": -1, "done": 0, "total": 0}))

    def test_failed_receipt_still_parses(self):
        raw = (b"BPR_STATUS_V3 ok=0 fw=13600007 boot=1791903645.738020 "
               b"base=ffffffffd7f10000 root=000000001bd64000 "
               b"stage=41505f434f4d504c hook=0000000000000000 "
               b"arm=6ace477b04d7a059 complete=6ace477b04d7a059 "
               b"gate=0 ist=0\n")
        parsed = watch.parse_receipt(raw)
        self.assertEqual(parsed["ok"], "0")
        self.assertEqual(parsed["arm"], "6ace477b04d7a059")

    def test_truncated_receipt_rejected(self):
        with self.assertRaises(ValueError):
            watch.parse_receipt(b"BPR_STATUS_V3 ok=1 arm=6ace477b04d7a059\n")

    def test_initial_countdown_sentinel_is_allowed(self):
        with patch.object(watch, "manager_get", return_value=b'{"remaining_ms":-1,"total":0,"done":0}'):
            self.assertEqual(watch.autoload_status("example")["remaining_ms"], -1)

    def test_invalid_autoload_counters_rejected(self):
        for payload in (
            b'{"remaining_ms":true,"total":0,"done":0}',
            b'{"remaining_ms":0,"total":1,"done":2}',
            b'{"remaining_ms":-2,"total":0,"done":0}',
        ):
            with self.subTest(payload=payload):
                with patch.object(watch, "manager_get", return_value=payload):
                    with self.assertRaises(ValueError):
                        watch.autoload_status("example")


if __name__ == "__main__":
    unittest.main()

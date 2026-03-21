#!/usr/bin/env python3
"""Regression test for Disk II P6 useful entry points and cycle timing."""

from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "scripts" / "test_diskii_p6_rom.py"


def _load_verify_module():
    spec = importlib.util.spec_from_file_location("test_diskii_p6_rom_module", MODULE_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable to load module from {MODULE_PATH}")
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod


class TestDiskIIP6EntryPoints(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.mod = _load_verify_module()
        cls.stock = (ROOT / "ROMS" / "DISKII_P6_STOCK.rom").read_bytes()
        cls.custom = (ROOT / "ROMS" / "DISKII_P6_CUSTOM.rom").read_bytes()

    def test_entry_point_signatures_are_pinned(self):
        self.mod.assert_entry_point_signatures(self.stock, "stock")
        self.mod.assert_entry_point_signatures(self.custom, "custom")

    def test_signature_bytes_are_pinned(self):
        self.mod.assert_pinned_bytes(self.stock, "stock")
        self.mod.assert_pinned_bytes(self.custom, "custom")

    def test_cycle_baselines_and_custom_parity(self):
        results = self.mod.measure_cycles(self.stock, self.custom)
        self.mod.assert_cycles(results)

    def test_verify_covers_all_useful_entry_points(self):
        # Guardrail: keep coverage broad if we add/remove useful entry points.
        self.assertGreaterEqual(len(self.mod.ENTRY_POINT_SIGNATURES), 7)


if __name__ == "__main__":
    unittest.main()

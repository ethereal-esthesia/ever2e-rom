#!/usr/bin/env python3
"""Regression test for the clean Disk II P6 substitute ROM."""

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
        cls.rom = cls.mod.load_rom()

    def test_boot_entry_is_pinned(self):
        self.mod.assert_boot_entry(self.rom)

    def test_signature_bytes_are_pinned(self):
        self.mod.assert_pinned_bytes(self.rom)

    def test_label_identifies_generated_substitute(self):
        self.mod.assert_label(self.rom)

    def test_boot_entry_self_loops_without_disk_io(self):
        self.mod.assert_boot_entry_is_inert(self.rom)


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""
Build a clean-room Disk II P6 substitute ROM for low-level slot/controller tests.

This image is intentionally not a functional Disk II boot ROM yet. It preserves
the common 16-sector identification bytes used by existing slot-ROM plumbing,
then falls into an inert self-loop. The JVM low-level disk write test exercises
controller soft switches directly and does not execute this ROM.
"""

from __future__ import annotations

import argparse
from pathlib import Path


ROM_SIZE = 0x100
ROM_LABEL = b"EVER2E P6 TEST ROM"

# A tiny, original boot-entry stub:
#   C600: LDX #$20
#   C602: LDY #$00
#   C604: LDX #$03
#   C606: STX $3C
#   C608: JMP $C608
BOOT_ENTRY = bytes(
    [
        0xA2,
        0x20,
        0xA0,
        0x00,
        0xA2,
        0x03,
        0x86,
        0x3C,
        0x4C,
        0x08,
        0xC6,
    ]
)

# Compatibility signature bytes used by broad Disk II/ProDOS detection.
COMPAT_SIGNATURE = {
    0x01: 0x20,
    0x03: 0x00,
    0x05: 0x03,
    0x07: 0x3C,
    0xFF: 0x00,
}


def build_bytes() -> bytearray:
    data = bytearray([0xEA] * ROM_SIZE)
    data[0 : len(BOOT_ENTRY)] = BOOT_ENTRY
    data[0x10 : 0x10 + len(ROM_LABEL)] = ROM_LABEL
    for off, val in COMPAT_SIGNATURE.items():
        data[off] = val
    if len(data) != ROM_SIZE:
        raise ValueError(f"Expected 256-byte ROM, got {len(data)}")
    return data


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--out",
        default="ROMS/DISKII_P6_CUSTOM.rom",
        help="Output ROM path",
    )
    parser.add_argument(
        "--stock-out",
        default=None,
        help="Deprecated; removed if present so stale stock artifacts do not linger",
    )
    args = parser.parse_args()

    custom = build_bytes()
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(custom)

    if args.stock_out:
        stock_path = Path(args.stock_out)
        if stock_path.exists():
            stock_path.unlink()

    print(f"Wrote {out_path} ({len(custom)} bytes)")
    print(
        "Pinned signature bytes:",
        ", ".join(f"${k:02X}=${v:02X}" for k, v in COMPAT_SIGNATURE.items()),
    )


if __name__ == "__main__":
    main()

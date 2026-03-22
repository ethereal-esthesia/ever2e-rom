#!/usr/bin/env python3
"""
Build a Disk II P6-compatible 256-byte ROM artifact while preserving
ProDOS-visible signature bytes.

This starts from a known-good stock image and enforces invariants so we can
iterate safely on internal bytes later.
"""

from pathlib import Path
import argparse


# Known-good 16-sector Disk II P6 ROM image (256 bytes).
STOCK_HEX = (
    "A220A000A203863C8A0A243CF010053C49FF297EB0084AD0FB989D5603C8E810"
    "E52058FFBABD00010A0A0A0A852BAABD8EC0BD8CC0BD8AC0BD89C0A050BD80C0"
    "9829030A052BAABD81C0A95620A8FC8810EB8526853D8541A90885271808BD8C"
    "C010FB49D5D0F7BD8CC010FBC9AAD0F3EABD8CC010FBC996F0092890DF49ADF0"
    "25D0D9A0038540BD8CC010FB2A853CBD8CC010FB253C88D0EC28C53DD0BEA540"
    "C541D0B8B0B7A056843CBC8CC010FB59D602A43C88990003D0EE843CBC8CC010"
    "FB59D602A43C9126C8D0EFBC8CC010FB59D602D087A000A256CA30FBB1265E00"
    "032A5E00032A9126C8D0EEE627E63DA53DCD0008A62B90DB4C01080000000000"
)

# Compatibility signature bytes for broad Disk II/ProDOS detection.
# We pin these bytes regardless of any future internal edits.
COMPAT_SIGNATURE = {
    0x01: 0x20,
    0x03: 0x00,
    0x05: 0x03,
    0x07: 0x3C,
    0xFF: 0x00,
}


def build_bytes() -> bytearray:
    data = bytearray.fromhex(STOCK_HEX)
    if len(data) != 256:
        raise ValueError(f"Expected 256-byte ROM, got {len(data)}")
    for off, val in COMPAT_SIGNATURE.items():
        data[off] = val
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
        default="ROMS/DISKII_P6_STOCK.rom",
        help="Optional stock ROM output path",
    )
    args = parser.parse_args()

    custom = build_bytes()
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(custom)

    stock_path = Path(args.stock_out)
    stock_path.parent.mkdir(parents=True, exist_ok=True)
    stock_path.write_bytes(bytearray.fromhex(STOCK_HEX))

    print(f"Wrote {out_path} ({len(custom)} bytes)")
    print(f"Wrote {stock_path} (256 bytes)")
    print(
        "Pinned signature bytes:",
        ", ".join(f"${k:02X}=${v:02X}" for k, v in COMPAT_SIGNATURE.items()),
    )


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
Build a clean-room Disk II P6 substitute ROM and matching boot-test disk.

This is not a DOS 3.3 or ProDOS-compatible Disk II boot ROM. It is a tiny,
original loader for a deliberately simple generated test stream:

  sync bytes, magic bytes, then 256 payload bytes encoded as high-bit nibbles.

That gives the emulators a bootable P6 path without carrying any proprietary
Apple, DOS, ProDOS, or third-party disk bytes.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path


ROM_BASE = 0xC600
ROM_SIZE = 0x100
PAYLOAD_ADDR = 0x0800
PAYLOAD_LOOP = PAYLOAD_ADDR + 0x0A
BOOT_MARK_ADDR = 0x0400

TRACK_TOTAL = 35
TRACK_BYTES = 416 * 16
BOOT_TRACK = 34
BOOT_SYNC_BYTES = 32
BOOT_MAGIC = bytes([0xE2, 0xB0, 0xB1, 0xB2])

# Compatibility signature bytes used by broad Disk II/ProDOS detection.
COMPAT_SIGNATURE = {
    0x01: 0x20,
    0x03: 0x00,
    0x05: 0x03,
    0x07: 0x3C,
    0xFF: 0x00,
}


@dataclass
class Patch:
    kind: str
    offset: int
    label: str


class Assembler:
    def __init__(self, origin: int) -> None:
        self.origin = origin
        self.data = bytearray()
        self.labels: dict[str, int] = {}
        self.patches: list[Patch] = []

    @property
    def pc(self) -> int:
        return self.origin + len(self.data)

    def label(self, name: str) -> None:
        if name in self.labels:
            raise ValueError(f"duplicate label {name!r}")
        self.labels[name] = self.pc

    def emit(self, *values: int) -> None:
        self.data.extend(value & 0xFF for value in values)

    def emit_abs(self, opcode: int, addr: int) -> None:
        self.emit(opcode, addr & 0xFF, (addr >> 8) & 0xFF)

    def jsr(self, label: str) -> None:
        self.emit(0x20, 0x00, 0x00)
        self.patches.append(Patch("abs", len(self.data) - 2, label))

    def branch(self, opcode: int, label: str) -> None:
        self.emit(opcode, 0x00)
        self.patches.append(Patch("rel", len(self.data) - 1, label))

    def finish(self) -> bytes:
        for patch in self.patches:
            if patch.label not in self.labels:
                raise ValueError(f"unknown label {patch.label!r}")
            target = self.labels[patch.label]
            if patch.kind == "abs":
                self.data[patch.offset] = target & 0xFF
                self.data[patch.offset + 1] = (target >> 8) & 0xFF
            elif patch.kind == "rel":
                next_pc = self.origin + patch.offset + 1
                delta = target - next_pc
                if not -128 <= delta <= 127:
                    raise ValueError(f"branch to {patch.label!r} is out of range")
                self.data[patch.offset] = delta & 0xFF
            else:
                raise ValueError(f"unknown patch kind {patch.kind!r}")
        return bytes(self.data)


def build_loader_code() -> bytes:
    asm = Assembler(ROM_BASE)

    # Preserve the common Disk II 16-sector identification bytes.
    asm.emit(
        0xA2,
        0x20,  # C600: LDX #$20
        0xA0,
        0x00,  # C602: LDY #$00
        0xA2,
        0x03,  # C604: LDX #$03
        0x86,
        0x3C,  # C606: STX $3C
    )

    # This clean-room test ROM is intentionally slot-6-only for now.
    asm.emit(0xA2, 0x60)  # LDX #$60
    asm.emit_abs(0xBD, 0xC089)  # LDA $C089,X ; drive 1 on at $C0E9
    asm.emit_abs(0xBD, 0xC08E)  # LDA $C08E,X ; read mode at $C0EE

    asm.label("find_magic")
    for magic_byte in BOOT_MAGIC:
        asm.jsr("read_disk_byte")
        asm.emit(0xC9, magic_byte)  # CMP #magic_byte
        asm.branch(0xD0, "find_magic")  # BNE find_magic

    asm.emit(0xA0, 0x00)  # LDY #$00

    asm.label("load_payload")
    asm.jsr("read_disk_byte")
    asm.emit(0x29, 0x0F)  # AND #$0F
    asm.emit(0x0A, 0x0A, 0x0A, 0x0A)  # ASL A four times
    asm.emit(0x85, 0x3D)  # STA $3D
    asm.jsr("read_disk_byte")
    asm.emit(0x29, 0x0F)  # AND #$0F
    asm.emit(0x05, 0x3D)  # ORA $3D
    asm.emit_abs(0x99, PAYLOAD_ADDR)  # STA PAYLOAD_ADDR,Y
    asm.emit(0xC8)  # INY
    asm.branch(0xD0, "load_payload")  # BNE load_payload
    asm.emit_abs(0x4C, PAYLOAD_ADDR)  # JMP PAYLOAD_ADDR

    asm.label("read_disk_byte")
    asm.emit_abs(0xBD, 0xC08C)  # LDA $C08C,X ; data latch at $C0EC
    asm.branch(0x10, "read_disk_byte")  # BPL read_disk_byte
    asm.emit(0x60)  # RTS

    return asm.finish()


def build_bytes() -> bytearray:
    code = build_loader_code()
    if len(code) >= ROM_SIZE:
        raise ValueError(f"loader is too large for P6 ROM: {len(code)} bytes")

    data = bytearray([0xEA] * ROM_SIZE)
    data[0 : len(code)] = code
    for off, val in COMPAT_SIGNATURE.items():
        data[off] = val
    if len(data) != ROM_SIZE:
        raise ValueError(f"Expected 256-byte ROM, got {len(data)}")
    return data


def build_boot_payload() -> bytes:
    payload = bytearray([0xEA] * 0x100)
    code = bytes(
        [
            0xA9,
            0x42,  # LDA #$42
            0x8D,
            BOOT_MARK_ADDR & 0xFF,
            (BOOT_MARK_ADDR >> 8) & 0xFF,  # STA $0400
            0xA9,
            0xC8,  # LDA #$C8
            0x8D,
            (BOOT_MARK_ADDR + 1) & 0xFF,
            ((BOOT_MARK_ADDR + 1) >> 8) & 0xFF,  # STA $0401
            0x4C,
            PAYLOAD_LOOP & 0xFF,
            (PAYLOAD_LOOP >> 8) & 0xFF,  # JMP $080A
        ]
    )
    payload[0 : len(code)] = code
    return bytes(payload)


def encode_payload(payload: bytes) -> bytes:
    encoded = bytearray()
    for value in payload:
        encoded.append(0xA0 | (value >> 4))
        encoded.append(0xA0 | (value & 0x0F))
    return bytes(encoded)


def decode_payload(encoded: bytes) -> bytes:
    if len(encoded) % 2:
        raise ValueError("encoded payload length must be even")
    payload = bytearray()
    for i in range(0, len(encoded), 2):
        payload.append(((encoded[i] & 0x0F) << 4) | (encoded[i + 1] & 0x0F))
    return bytes(payload)


def build_boot_test_nib() -> bytearray:
    image = bytearray([0xFF] * (TRACK_TOTAL * TRACK_BYTES))
    stream = bytes([0xFF] * BOOT_SYNC_BYTES) + BOOT_MAGIC + encode_payload(build_boot_payload())
    if len(stream) > TRACK_BYTES:
        raise ValueError(f"boot stream is too large for one NIB track: {len(stream)} bytes")
    start = BOOT_TRACK * TRACK_BYTES
    image[start : start + len(stream)] = stream
    return image


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--out",
        default="ROMS/DISKII_P6_CUSTOM.rom",
        help="Output ROM path",
    )
    parser.add_argument(
        "--boot-test-out",
        default="ROMS/DISKII_P6_BOOT_TEST.nib",
        help="Output generated custom boot-test NIB path",
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

    boot_path = Path(args.boot_test_out)
    boot_path.parent.mkdir(parents=True, exist_ok=True)
    boot = build_boot_test_nib()
    boot_path.write_bytes(boot)

    if args.stock_out:
        stock_path = Path(args.stock_out)
        if stock_path.exists():
            stock_path.unlink()

    print(f"Wrote {out_path} ({len(custom)} bytes)")
    print(f"Wrote {boot_path} ({len(boot)} bytes)")
    print(
        "Pinned signature bytes:",
        ", ".join(f"${k:02X}=${v:02X}" for k, v in COMPAT_SIGNATURE.items()),
    )


if __name__ == "__main__":
    main()

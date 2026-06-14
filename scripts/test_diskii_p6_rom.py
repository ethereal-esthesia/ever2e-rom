#!/usr/bin/env python3
"""Validate the clean-room Disk II P6 substitute ROM and custom boot disk."""

from __future__ import annotations

import importlib.util
import itertools
import os
import sys
from pathlib import Path


ROM_BASE = 0xC600
CUSTOM_ROM = Path("ROMS/DISKII_P6_CUSTOM.rom")
CUSTOM_STREAM_ROM = Path("ROMS/DISKII_P6_CUSTOM_STREAM.rom")
CUSTOM_BOOT_NIB = Path("ROMS/DISKII_P6_BOOT_TEST.nib")
EXPECTED_SIZE = 0x100
EXPECTED_BOOT_ENTRY_PREFIX = [0xA2, 0x20, 0xA0, 0x00, 0xA2, 0x03, 0x86, 0x3C, 0x8A, 0x0A]
EXECUTABLE_PREFIX_LEN = 0xFB

PINNED_BYTES = {
    0x01: 0x20,
    0x03: 0x00,
    0x05: 0x03,
    0x07: 0x3C,
    0xFF: 0x00,
}


def _load_builder_module():
    module_path = Path(__file__).with_name("build_diskii_p6_custom.py")
    spec = importlib.util.spec_from_file_location("build_diskii_p6_custom_module", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable to load module from {module_path}")
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod


BUILD = _load_builder_module()


def load_rom() -> bytes:
    return CUSTOM_ROM.read_bytes()


def load_custom_stream_rom() -> bytes:
    return CUSTOM_STREAM_ROM.read_bytes()


def load_boot_nib() -> bytes:
    return CUSTOM_BOOT_NIB.read_bytes()


def assert_pinned_bytes(rom: bytes) -> None:
    if len(rom) != EXPECTED_SIZE:
        raise AssertionError(f"expected 256 bytes, got {len(rom)}")
    for off, expected in PINNED_BYTES.items():
        got = rom[off]
        if got != expected:
            raise AssertionError(f"byte ${off:02X} expected ${expected:02X}, got ${got:02X}")


def assert_boot_entry(rom: bytes) -> None:
    got = list(rom[0 : len(EXPECTED_BOOT_ENTRY_PREFIX)])
    if got != EXPECTED_BOOT_ENTRY_PREFIX:
        raise AssertionError(
            "boot entry changed: "
            f"expected {[f'${v:02X}' for v in EXPECTED_BOOT_ENTRY_PREFIX]}, "
            f"got {[f'${v:02X}' for v in got]}"
        )


def assert_not_local_stock_clone_if_available(rom: bytes) -> None:
    stock_env = os.environ.get("STOCK_P6_ROM", "")
    candidates = [Path(stock_env)] if stock_env else []
    candidates.append(Path("/Users/shane/Project/ever2e-cpp/release/unused/SLOT6.PROM"))
    stock_path = next((path for path in candidates if path.exists()), None)
    if stock_path is None:
        return
    stock = stock_path.read_bytes()
    if len(stock) != EXPECTED_SIZE:
        raise AssertionError(f"stock P6 ROM must be 256 bytes: {stock_path}")
    if rom[:EXECUTABLE_PREFIX_LEN] == stock[:EXECUTABLE_PREFIX_LEN]:
        raise AssertionError("generated P6 ROM executable bytes match local stock PROM")


def assert_boot_test_disk(nib: bytes) -> None:
    expected_size = BUILD.TRACK_TOTAL * BUILD.TRACK_BYTES
    if len(nib) != expected_size:
        raise AssertionError(f"expected {expected_size} NIB bytes, got {len(nib)}")

    track_start = BUILD.BOOT_TRACK * BUILD.TRACK_BYTES
    magic_start = track_start + BUILD.BOOT_SYNC_BYTES
    got_magic = nib[magic_start : magic_start + len(BUILD.BOOT_MAGIC)]
    if got_magic != BUILD.BOOT_MAGIC:
        raise AssertionError(f"boot magic changed: expected {BUILD.BOOT_MAGIC!r}, got {got_magic!r}")

    encoded_start = magic_start + len(BUILD.BOOT_MAGIC)
    encoded_end = encoded_start + 0x200
    decoded = BUILD.decode_payload(nib[encoded_start:encoded_end])
    expected_payload = BUILD.build_boot_payload()
    if decoded != expected_payload:
        raise AssertionError("encoded boot payload does not decode to the generated payload")


class Mini6502:
    def __init__(self, rom: bytes, disk_stream: bytes) -> None:
        self.memory = bytearray(0x10000)
        self.memory[ROM_BASE : ROM_BASE + len(rom)] = rom
        self.disk_stream = itertools.cycle(disk_stream)
        self.pc = ROM_BASE
        self.a = 0
        self.x = 0
        self.y = 0
        self.z = False
        self.n = False
        self.call_stack: list[int] = []

    def rd(self, addr: int) -> int:
        addr &= 0xFFFF
        if addr == 0xC0EC:
            return next(self.disk_stream)
        if addr in (0xC0E9, 0xC0EE):
            return 0
        return self.memory[addr]

    def wr(self, addr: int, value: int) -> None:
        self.memory[addr & 0xFFFF] = value & 0xFF

    def set_nz(self, value: int) -> None:
        value &= 0xFF
        self.z = value == 0
        self.n = bool(value & 0x80)

    def read_pc(self) -> int:
        value = self.rd(self.pc)
        self.pc = (self.pc + 1) & 0xFFFF
        return value

    def read_abs_operand(self) -> int:
        lo = self.read_pc()
        hi = self.read_pc()
        return ((hi << 8) | lo) & 0xFFFF

    def branch(self, condition: bool) -> None:
        offset = self.read_pc()
        if condition:
            if offset & 0x80:
                offset -= 0x100
            self.pc = (self.pc + offset) & 0xFFFF

    def step(self) -> None:
        op_addr = self.pc
        op = self.read_pc()
        if op == 0xA2:  # LDX #imm
            self.x = self.read_pc()
            self.set_nz(self.x)
        elif op == 0xA0:  # LDY #imm
            self.y = self.read_pc()
            self.set_nz(self.y)
        elif op == 0xA9:  # LDA #imm
            self.a = self.read_pc()
            self.set_nz(self.a)
        elif op == 0x86:  # STX zp
            self.wr(self.read_pc(), self.x)
        elif op == 0x85:  # STA zp
            self.wr(self.read_pc(), self.a)
        elif op == 0x8D:  # STA abs
            self.wr(self.read_abs_operand(), self.a)
        elif op == 0x99:  # STA abs,Y
            self.wr((self.read_abs_operand() + self.y) & 0xFFFF, self.a)
        elif op == 0xBD:  # LDA abs,X
            self.a = self.rd((self.read_abs_operand() + self.x) & 0xFFFF)
            self.set_nz(self.a)
        elif op == 0xC9:  # CMP #imm
            value = self.read_pc()
            result = (self.a - value) & 0xFF
            self.z = self.a == value
            self.n = bool(result & 0x80)
        elif op == 0x29:  # AND #imm
            self.a &= self.read_pc()
            self.set_nz(self.a)
        elif op == 0x05:  # ORA zp
            self.a |= self.rd(self.read_pc())
            self.set_nz(self.a)
        elif op == 0x0A:  # ASL A
            self.a = (self.a << 1) & 0xFF
            self.set_nz(self.a)
        elif op == 0xC8:  # INY
            self.y = (self.y + 1) & 0xFF
            self.set_nz(self.y)
        elif op == 0xD0:  # BNE rel
            self.branch(not self.z)
        elif op == 0x10:  # BPL rel
            self.branch(not self.n)
        elif op == 0x20:  # JSR abs
            target = self.read_abs_operand()
            self.call_stack.append(self.pc)
            self.pc = target
        elif op == 0x60:  # RTS
            if not self.call_stack:
                raise AssertionError(f"RTS with empty call stack at ${op_addr:04X}")
            self.pc = self.call_stack.pop()
        elif op == 0x4C:  # JMP abs
            self.pc = self.read_abs_operand()
        elif op == 0xEA:  # NOP
            pass
        else:
            raise AssertionError(f"unexpected opcode ${op:02X} at ${op_addr:04X}")


def run_custom_boot(rom: bytes, nib: bytes, max_steps: int = 25000) -> Mini6502:
    track_start = BUILD.BOOT_TRACK * BUILD.TRACK_BYTES
    track = nib[track_start : track_start + BUILD.TRACK_BYTES]
    cpu = Mini6502(rom, track)

    for _ in range(max_steps):
        if (
            cpu.pc == BUILD.PAYLOAD_LOOP
            and cpu.memory[BUILD.BOOT_MARK_ADDR] == 0x42
            and cpu.memory[BUILD.BOOT_MARK_ADDR + 1] == 0xC8
        ):
            return cpu
        cpu.step()

    raise AssertionError(
        "custom boot did not reach success loop; "
        f"PC=${cpu.pc:04X}, $0400=${cpu.memory[BUILD.BOOT_MARK_ADDR]:02X}, "
        f"$0401=${cpu.memory[BUILD.BOOT_MARK_ADDR + 1]:02X}"
    )


def assert_custom_boots(rom: bytes, nib: bytes) -> None:
    cpu = run_custom_boot(rom, nib)
    expected_payload = BUILD.build_boot_payload()
    got_payload = bytes(cpu.memory[BUILD.PAYLOAD_ADDR : BUILD.PAYLOAD_ADDR + 0x100])
    if got_payload != expected_payload:
        raise AssertionError("P6 loader did not copy the generated payload exactly")


def verify(rom: bytes, custom_stream_rom: bytes, nib: bytes) -> None:
    assert_pinned_bytes(rom)
    assert_boot_entry(rom)
    assert_not_local_stock_clone_if_available(rom)
    assert_boot_test_disk(nib)
    assert_custom_boots(custom_stream_rom, nib)


def main() -> None:
    rom = load_rom()
    custom_stream_rom = load_custom_stream_rom()
    nib = load_boot_nib()
    verify(rom, custom_stream_rom, nib)
    print("PASS: Disk II P6 substitute ROM verified")
    print("  standard ROM: local stock PROM clone guard passes when present")
    print("  custom stream fixture: generated NIB stream loads payload to $0800")


if __name__ == "__main__":
    main()

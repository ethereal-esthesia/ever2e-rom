#!/usr/bin/env python3
"""Validate the clean-room Disk II P6 substitute ROM artifact."""

from __future__ import annotations

from pathlib import Path


ROM_BASE = 0xC600
CUSTOM_ROM = Path("ROMS/DISKII_P6_CUSTOM.rom")
EXPECTED_SIZE = 0x100
EXPECTED_BOOT_ENTRY = [0xA2, 0x20, 0xA0, 0x00, 0xA2, 0x03, 0x86, 0x3C, 0x4C, 0x08, 0xC6]
EXPECTED_LABEL = b"EVER2E P6 TEST ROM"

PINNED_BYTES = {
    0x01: 0x20,
    0x03: 0x00,
    0x05: 0x03,
    0x07: 0x3C,
    0xFF: 0x00,
}


def load_rom() -> bytes:
    return CUSTOM_ROM.read_bytes()


def assert_pinned_bytes(rom: bytes) -> None:
    if len(rom) != EXPECTED_SIZE:
        raise AssertionError(f"expected 256 bytes, got {len(rom)}")
    for off, expected in PINNED_BYTES.items():
        got = rom[off]
        if got != expected:
            raise AssertionError(f"byte ${off:02X} expected ${expected:02X}, got ${got:02X}")


def assert_boot_entry(rom: bytes) -> None:
    got = list(rom[0 : len(EXPECTED_BOOT_ENTRY)])
    if got != EXPECTED_BOOT_ENTRY:
        raise AssertionError(
            "boot entry changed: "
            f"expected {[f'${v:02X}' for v in EXPECTED_BOOT_ENTRY]}, "
            f"got {[f'${v:02X}' for v in got]}"
        )


def assert_label(rom: bytes) -> None:
    got = rom[0x10 : 0x10 + len(EXPECTED_LABEL)]
    if got != EXPECTED_LABEL:
        raise AssertionError(f"label changed: expected {EXPECTED_LABEL!r}, got {got!r}")


def run_boot_entry(rom: bytes, max_steps: int = 8) -> tuple[int, dict[int, int]]:
    a = 0
    x = 0
    y = 0
    pc = ROM_BASE
    memory: dict[int, int] = {}

    def rd(addr: int) -> int:
        if ROM_BASE <= addr < ROM_BASE + len(rom):
            return rom[addr - ROM_BASE]
        return memory.get(addr & 0xFFFF, 0)

    def wr(addr: int, value: int) -> None:
        memory[addr & 0xFFFF] = value & 0xFF

    for _ in range(max_steps):
        op = rd(pc)
        pc = (pc + 1) & 0xFFFF
        if op == 0xA2:  # LDX #imm
            x = rd(pc)
            pc = (pc + 1) & 0xFFFF
        elif op == 0xA0:  # LDY #imm
            y = rd(pc)
            pc = (pc + 1) & 0xFFFF
        elif op == 0x86:  # STX zp
            zp = rd(pc)
            pc = (pc + 1) & 0xFFFF
            wr(zp, x)
        elif op == 0x4C:  # JMP abs
            lo = rd(pc)
            hi = rd((pc + 1) & 0xFFFF)
            pc = ((hi << 8) | lo) & 0xFFFF
        else:
            raise AssertionError(f"unexpected opcode ${op:02X} at ${((pc - 1) & 0xFFFF):04X}")
        _ = a, y
    return pc, memory


def assert_boot_entry_is_inert(rom: bytes) -> None:
    pc, memory = run_boot_entry(rom)
    if pc != ROM_BASE + 0x08:
        raise AssertionError(f"expected self-loop at ${ROM_BASE + 0x08:04X}, got PC=${pc:04X}")
    if memory.get(0x3C) != 0x03:
        raise AssertionError(f"expected boot stub to set $3C=$03, got {memory.get(0x3C)!r}")


def verify(rom: bytes) -> None:
    assert_pinned_bytes(rom)
    assert_boot_entry(rom)
    assert_label(rom)
    assert_boot_entry_is_inert(rom)


def main() -> None:
    rom = load_rom()
    verify(rom)
    print("PASS: clean Disk II P6 substitute ROM verified")
    print("  boot entry: C600 signature plus C608 self-loop")


if __name__ == "__main__":
    main()

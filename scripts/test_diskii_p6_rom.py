#!/usr/bin/env python3
"""
Checks Disk II P6 stock/custom ROM compatibility invariants:
1) Signature bytes used by ProDOS/Disk II detection.
2) Cycle counts for critical timing-sensitive code paths.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


ROM_BASE = 0xC600

STOCK_ROM = Path("ROMS/DISKII_P6_STOCK.rom")
CUSTOM_ROM = Path("ROMS/DISKII_P6_CUSTOM.rom")

# ProDOS + common Disk II detection bytes to pin.
PINNED_BYTES = {
    0x01: 0x20,
    0x03: 0x00,
    0x05: 0x03,
    0x07: 0x3C,
    0xFF: 0x00,  # 16-sector style.
}


@dataclass
class Cpu:
    a: int = 0
    x: int = 0
    y: int = 0
    sp: int = 0xFF
    pc: int = ROM_BASE
    p: int = 0x20  # bit5 set on 6502
    cycles: int = 0

    def set_nz(self, val: int) -> None:
        val &= 0xFF
        self.p = (self.p & ~0x82) | (0x80 if (val & 0x80) else 0) | (0x02 if val == 0 else 0)

    def set_c(self, c: bool) -> None:
        if c:
            self.p |= 0x01
        else:
            self.p &= ~0x01

    def c(self) -> int:
        return self.p & 0x01

    def z(self) -> int:
        return self.p & 0x02

    def n(self) -> int:
        return self.p & 0x80


def branch(cpu: Cpu, taken: bool, offset: int) -> None:
    cpu.cycles += 2
    if not taken:
        return
    old_pc = cpu.pc
    rel = offset if offset < 0x80 else offset - 0x100
    cpu.pc = (cpu.pc + rel) & 0xFFFF
    cpu.cycles += 1
    if (old_pc & 0xFF00) != (cpu.pc & 0xFF00):
        cpu.cycles += 1


def run_path(rom: bytes, start_off: int, stop_off: int, c08c_reads: list[int], max_steps: int = 10000) -> int:
    mem = [0] * 65536
    for i, b in enumerate(rom):
        mem[ROM_BASE + i] = b
    read_queue = list(c08c_reads)
    cpu = Cpu(pc=ROM_BASE + start_off, x=0)

    def rd(addr: int) -> int:
        if addr == 0xC08C:
            if read_queue:
                return read_queue.pop(0) & 0xFF
            return 0x80
        return mem[addr & 0xFFFF]

    def wr(addr: int, val: int) -> None:
        mem[addr & 0xFFFF] = val & 0xFF

    steps = 0
    stop_pc = ROM_BASE + stop_off
    while cpu.pc != stop_pc:
        if steps >= max_steps:
            raise RuntimeError(f"step limit exceeded at PC=${cpu.pc:04X}")
        steps += 1
        op = rd(cpu.pc)
        cpu.pc = (cpu.pc + 1) & 0xFFFF

        if op == 0x18:  # CLC
            cpu.set_c(False)
            cpu.cycles += 2
        elif op == 0x08:  # PHP
            wr(0x100 + cpu.sp, cpu.p | 0x10)
            cpu.sp = (cpu.sp - 1) & 0xFF
            cpu.cycles += 3
        elif op == 0x28:  # PLP
            cpu.sp = (cpu.sp + 1) & 0xFF
            cpu.p = (rd(0x100 + cpu.sp) | 0x20) & 0xEF  # keep bit5, clear break in live P
            cpu.cycles += 4
        elif op == 0xBD:  # LDA abs,X
            lo = rd(cpu.pc)
            hi = rd(cpu.pc + 1)
            cpu.pc = (cpu.pc + 2) & 0xFFFF
            base = (hi << 8) | lo
            addr = (base + cpu.x) & 0xFFFF
            cpu.a = rd(addr)
            cpu.set_nz(cpu.a)
            cpu.cycles += 4
            if (base & 0xFF00) != (addr & 0xFF00):
                cpu.cycles += 1
        elif op == 0x49:  # EOR #imm
            v = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            cpu.a = (cpu.a ^ v) & 0xFF
            cpu.set_nz(cpu.a)
            cpu.cycles += 2
        elif op == 0xC9:  # CMP #imm
            v = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            t = (cpu.a - v) & 0x1FF
            cpu.set_c(cpu.a >= v)
            cpu.set_nz(t & 0xFF)
            cpu.cycles += 2
        elif op == 0xEA:  # NOP
            cpu.cycles += 2
        elif op == 0x10:  # BPL
            off = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            branch(cpu, cpu.n() == 0, off)
        elif op == 0xD0:  # BNE
            off = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            branch(cpu, cpu.z() == 0, off)
        elif op == 0xF0:  # BEQ
            off = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            branch(cpu, cpu.z() != 0, off)
        elif op == 0x90:  # BCC
            off = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            branch(cpu, cpu.c() == 0, off)
        else:
            raise RuntimeError(f"unsupported opcode ${op:02X} at ${((cpu.pc - 1) & 0xFFFF):04X}")

    return cpu.cycles


def assert_pinned_bytes(rom: bytes, name: str) -> None:
    if len(rom) != 256:
        raise AssertionError(f"{name}: expected 256 bytes, got {len(rom)}")
    for off, expected in PINNED_BYTES.items():
        got = rom[off]
        if got != expected:
            raise AssertionError(f"{name}: byte ${off:02X} expected ${expected:02X}, got ${got:02X}")


def main() -> None:
    stock = STOCK_ROM.read_bytes()
    custom = CUSTOM_ROM.read_bytes()

    assert_pinned_bytes(stock, "stock")
    assert_pinned_bytes(custom, "custom")

    # Critical path 1:
    # Sync prologue path: find D5,AA,96 and reach LFF83 (offset $83).
    # Entry at Cn5C.
    sync_reads = [0xD5, 0xAA, 0x96]
    stock_sync_cycles = run_path(stock, 0x5C, 0x83, sync_reads)
    custom_sync_cycles = run_path(custom, 0x5C, 0x83, sync_reads)

    # Critical path 2:
    # Mismatch recovery path: D5,AA,AD should pop flags and branch back to Cn5C.
    mismatch_reads = [0xD5, 0xAA, 0xAD]
    stock_mismatch_cycles = run_path(stock, 0x5D, 0x5C, mismatch_reads)
    custom_mismatch_cycles = run_path(custom, 0x5D, 0x5C, mismatch_reads)

    # Fixed cycle expectations from the reference stock ROM.
    EXPECT_SYNC = 38
    EXPECT_MISMATCH = 42

    if stock_sync_cycles != EXPECT_SYNC:
        raise AssertionError(f"stock sync cycles changed: {stock_sync_cycles} != {EXPECT_SYNC}")
    if stock_mismatch_cycles != EXPECT_MISMATCH:
        raise AssertionError(f"stock mismatch cycles changed: {stock_mismatch_cycles} != {EXPECT_MISMATCH}")

    if custom_sync_cycles != stock_sync_cycles:
        raise AssertionError(
            f"custom sync cycles differ: custom={custom_sync_cycles}, stock={stock_sync_cycles}"
        )
    if custom_mismatch_cycles != stock_mismatch_cycles:
        raise AssertionError(
            f"custom mismatch cycles differ: custom={custom_mismatch_cycles}, stock={stock_mismatch_cycles}"
        )

    print("PASS: signature bytes and critical path cycle counts verified")
    print(
        f"  sync_path_cycles={custom_sync_cycles}, mismatch_path_cycles={custom_mismatch_cycles}"
    )


if __name__ == "__main__":
    main()

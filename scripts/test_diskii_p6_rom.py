#!/usr/bin/env python3
"""
Checks Disk II P6 stock/custom ROM compatibility invariants:
1) Signature bytes used by ProDOS/Disk II detection.
2) Entry point signature bytes for useful external/internal hooks.
3) Cycle counts for critical timing-sensitive code paths.
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

# Treat the first 8 bytes as a compact controller fingerprint.
# This includes the classic ProDOS-identifying bytes at $01/$03/$05/$07.
PRODOS_FINGERPRINT_00_07 = [0xA2, 0x20, 0xA0, 0x00, 0xA2, 0x03, 0x86, 0x3C]

# Useful entry points we rely on for compatibility checks and timing tests.
# We pin short leading byte signatures to spot accidental regressions.
ENTRY_POINT_SIGNATURES = {
    0x00: [0xA2, 0x20, 0xA0, 0x00, 0xA2, 0x03, 0x86, 0x3C],  # Boot entry
    0x5C: [0x18, 0x08, 0xBD, 0x8C, 0xC0, 0x10, 0xFB, 0x49],  # Sync prologue
    0x5D: [0x08, 0xBD, 0x8C, 0xC0, 0x10, 0xFB, 0x49, 0xD5],  # Sync mismatch recovery
    0xA6: [0xA0, 0x56, 0x84, 0x3C, 0xBC, 0x8C, 0xC0, 0x10],  # Decode to $0300 entry
    0xBA: [0x84, 0x3C, 0xBC, 0x8C, 0xC0, 0x10, 0xFB, 0x59],  # Decode to ($26),Y entry
    0xCB: [0xBC, 0x8C, 0xC0, 0x10, 0xFB, 0x59, 0xD6, 0x02],  # Decode tail entry
    0xD7: [0xA2, 0x56, 0xCA, 0x30, 0xFB, 0xB1, 0x26, 0x5E],  # Bit-pack loop entry
}

# Baseline stock timings for timing-sensitive paths.
EXPECTED_CYCLES = {
    "sync": 38,
    "mismatch": 42,
    "sync_spins": 59,
    "decode_0300": 2323,
    "decode_dst": 7167,
    "pack": 9766,
    "decode_tail": 22,
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


def run_path(
    rom: bytes,
    start_off: int,
    stop_off: int,
    c08c_reads: list[int],
    *,
    init_a: int = 0,
    init_x: int = 0,
    init_y: int = 0,
    init_sp: int = 0xFF,
    init_p: int = 0x20,
    init_mem: dict[int, int] | None = None,
    c08c_default: int = 0x80,
    max_steps: int = 10000,
) -> int:
    mem = [0] * 65536
    for i, b in enumerate(rom):
        mem[ROM_BASE + i] = b
    if init_mem:
        for addr, val in init_mem.items():
            mem[addr & 0xFFFF] = val & 0xFF
    read_queue = list(c08c_reads)
    cpu = Cpu(
        a=init_a & 0xFF,
        x=init_x & 0xFF,
        y=init_y & 0xFF,
        sp=init_sp & 0xFF,
        pc=ROM_BASE + start_off,
        p=init_p & 0xFF,
    )

    def rd(addr: int) -> int:
        if addr == 0xC08C:
            if read_queue:
                return read_queue.pop(0) & 0xFF
            return c08c_default & 0xFF
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
            cpu.p = (rd(0x100 + cpu.sp) | 0x20) & 0xEF
            cpu.cycles += 4
        elif op == 0xA0:  # LDY #imm
            cpu.y = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            cpu.set_nz(cpu.y)
            cpu.cycles += 2
        elif op == 0xA2:  # LDX #imm
            cpu.x = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            cpu.set_nz(cpu.x)
            cpu.cycles += 2
        elif op == 0xA4:  # LDY zp
            zp = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            cpu.y = rd(zp)
            cpu.set_nz(cpu.y)
            cpu.cycles += 3
        elif op == 0xA5:  # LDA zp
            zp = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            cpu.a = rd(zp)
            cpu.set_nz(cpu.a)
            cpu.cycles += 3
        elif op == 0xA6:  # LDX zp
            zp = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            cpu.x = rd(zp)
            cpu.set_nz(cpu.x)
            cpu.cycles += 3
        elif op == 0x2A:  # ROL A
            c_in = cpu.c()
            c_out = 1 if (cpu.a & 0x80) else 0
            cpu.a = ((cpu.a << 1) & 0xFF) | c_in
            cpu.set_c(c_out != 0)
            cpu.set_nz(cpu.a)
            cpu.cycles += 2
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
        elif op == 0xBC:  # LDY abs,X
            lo = rd(cpu.pc)
            hi = rd(cpu.pc + 1)
            cpu.pc = (cpu.pc + 2) & 0xFFFF
            base = (hi << 8) | lo
            addr = (base + cpu.x) & 0xFFFF
            cpu.y = rd(addr)
            cpu.set_nz(cpu.y)
            cpu.cycles += 4
            if (base & 0xFF00) != (addr & 0xFF00):
                cpu.cycles += 1
        elif op == 0xB1:  # LDA (zp),Y
            zp = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            lo = rd(zp)
            hi = rd((zp + 1) & 0xFF)
            base = (hi << 8) | lo
            addr = (base + cpu.y) & 0xFFFF
            cpu.a = rd(addr)
            cpu.set_nz(cpu.a)
            cpu.cycles += 5
            if (base & 0xFF00) != (addr & 0xFF00):
                cpu.cycles += 1
        elif op == 0x49:  # EOR #imm
            v = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            cpu.a = (cpu.a ^ v) & 0xFF
            cpu.set_nz(cpu.a)
            cpu.cycles += 2
        elif op == 0x59:  # EOR abs,Y
            lo = rd(cpu.pc)
            hi = rd(cpu.pc + 1)
            cpu.pc = (cpu.pc + 2) & 0xFFFF
            base = (hi << 8) | lo
            addr = (base + cpu.y) & 0xFFFF
            cpu.a = (cpu.a ^ rd(addr)) & 0xFF
            cpu.set_nz(cpu.a)
            cpu.cycles += 4
            if (base & 0xFF00) != (addr & 0xFF00):
                cpu.cycles += 1
        elif op == 0x5E:  # LSR abs,X
            lo = rd(cpu.pc)
            hi = rd(cpu.pc + 1)
            cpu.pc = (cpu.pc + 2) & 0xFFFF
            addr = (((hi << 8) | lo) + cpu.x) & 0xFFFF
            v = rd(addr)
            cpu.set_c((v & 0x01) != 0)
            v = (v >> 1) & 0xFF
            wr(addr, v)
            cpu.set_nz(v)
            cpu.cycles += 7
        elif op == 0x84:  # STY zp
            zp = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            wr(zp, cpu.y)
            cpu.cycles += 3
        elif op == 0x88:  # DEY
            cpu.y = (cpu.y - 1) & 0xFF
            cpu.set_nz(cpu.y)
            cpu.cycles += 2
        elif op == 0x91:  # STA (zp),Y
            zp = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            lo = rd(zp)
            hi = rd((zp + 1) & 0xFF)
            base = (hi << 8) | lo
            addr = (base + cpu.y) & 0xFFFF
            wr(addr, cpu.a)
            cpu.cycles += 6
        elif op == 0x99:  # STA abs,Y
            lo = rd(cpu.pc)
            hi = rd(cpu.pc + 1)
            cpu.pc = (cpu.pc + 2) & 0xFFFF
            addr = (((hi << 8) | lo) + cpu.y) & 0xFFFF
            wr(addr, cpu.a)
            cpu.cycles += 5
        elif op == 0xC8:  # INY
            cpu.y = (cpu.y + 1) & 0xFF
            cpu.set_nz(cpu.y)
            cpu.cycles += 2
        elif op == 0xC9:  # CMP #imm
            v = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            t = (cpu.a - v) & 0x1FF
            cpu.set_c(cpu.a >= v)
            cpu.set_nz(t & 0xFF)
            cpu.cycles += 2
        elif op == 0xCD:  # CMP abs
            lo = rd(cpu.pc)
            hi = rd(cpu.pc + 1)
            cpu.pc = (cpu.pc + 2) & 0xFFFF
            v = rd((hi << 8) | lo)
            t = (cpu.a - v) & 0x1FF
            cpu.set_c(cpu.a >= v)
            cpu.set_nz(t & 0xFF)
            cpu.cycles += 4
        elif op == 0xCA:  # DEX
            cpu.x = (cpu.x - 1) & 0xFF
            cpu.set_nz(cpu.x)
            cpu.cycles += 2
        elif op == 0xD0:  # BNE
            off = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            branch(cpu, cpu.z() == 0, off)
        elif op == 0xE6:  # INC zp
            zp = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            v = (rd(zp) + 1) & 0xFF
            wr(zp, v)
            cpu.set_nz(v)
            cpu.cycles += 5
        elif op == 0xEA:  # NOP
            cpu.cycles += 2
        elif op == 0xF0:  # BEQ
            off = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            branch(cpu, cpu.z() != 0, off)
        elif op == 0x10:  # BPL
            off = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            branch(cpu, cpu.n() == 0, off)
        elif op == 0x30:  # BMI
            off = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            branch(cpu, cpu.n() != 0, off)
        elif op == 0x90:  # BCC
            off = rd(cpu.pc)
            cpu.pc = (cpu.pc + 1) & 0xFFFF
            branch(cpu, cpu.c() == 0, off)
        elif op == 0x4C:  # JMP abs
            lo = rd(cpu.pc)
            hi = rd(cpu.pc + 1)
            cpu.pc = ((hi << 8) | lo) & 0xFFFF
            cpu.cycles += 3
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


def assert_entry_point_signatures(rom: bytes, name: str) -> None:
    for off, expected in ENTRY_POINT_SIGNATURES.items():
        got = list(rom[off:off + len(expected)])
        if got != expected:
            raise AssertionError(
                f"{name}: entry ${off:02X} bytes changed: "
                f"expected {[f'${v:02X}' for v in expected]}, got {[f'${v:02X}' for v in got]}"
            )


def assert_prodos_fingerprint(rom: bytes, name: str) -> None:
    got = list(rom[0x00:0x08])
    if got != PRODOS_FINGERPRINT_00_07:
        raise AssertionError(
            f"{name}: ProDOS fingerprint $00..$07 changed: "
            f"expected {[f'${v:02X}' for v in PRODOS_FINGERPRINT_00_07]}, "
            f"got {[f'${v:02X}' for v in got]}"
        )


def measure_cycles(stock: bytes, custom: bytes) -> dict[str, int]:
    out: dict[str, int] = {}

    # Critical path 1: sync prologue (Cn5C -> Cn83)
    out["stock_sync"] = run_path(stock, 0x5C, 0x83, [0xD5, 0xAA, 0x96])
    out["custom_sync"] = run_path(custom, 0x5C, 0x83, [0xD5, 0xAA, 0x96])

    # Critical path 2: mismatch recovery (Cn5D -> Cn5C)
    out["stock_mismatch"] = run_path(stock, 0x5D, 0x5C, [0xD5, 0xAA, 0xAD])
    out["custom_mismatch"] = run_path(custom, 0x5D, 0x5C, [0xD5, 0xAA, 0xAD])
    # Sync prologue with one explicit wait-loop spin before each match byte.
    spin_reads = [0x00, 0xD5, 0x00, 0xAA, 0x00, 0x96]
    out["stock_sync_spins"] = run_path(stock, 0x5C, 0x83, spin_reads.copy())
    out["custom_sync_spins"] = run_path(custom, 0x5C, 0x83, spin_reads.copy())

    # Critical path 3: decode to $0300 buffer (CnA6 -> CnBA)
    out["stock_decode_0300"] = run_path(stock, 0xA6, 0xBA, [0x80] * 0x56, init_x=0, init_a=0)
    out["custom_decode_0300"] = run_path(custom, 0xA6, 0xBA, [0x80] * 0x56, init_x=0, init_a=0)

    # Critical path 4: decode/store to ($26),Y buffer (CnBA -> CnCB)
    init_ptr = {0x26: 0x00, 0x27: 0x04}
    out["stock_decode_dst"] = run_path(stock, 0xBA, 0xCB, [0x80] * 0x100, init_x=0, init_a=0, init_y=0, init_mem=init_ptr)
    out["custom_decode_dst"] = run_path(custom, 0xBA, 0xCB, [0x80] * 0x100, init_x=0, init_a=0, init_y=0, init_mem=init_ptr)

    # Critical path 5: bit-pack loop pass (CnD7 -> CnD3)
    init_pack = {0x26: 0x00, 0x27: 0x04, 0x3D: 0x00, 0x0800: 0xFF, 0x2B: 0x00}
    out["stock_pack"] = run_path(stock, 0xD7, 0xD3, [], init_mem=init_pack, max_steps=500000)
    out["custom_pack"] = run_path(custom, 0xD7, 0xD3, [], init_mem=init_pack, max_steps=500000)
    # Decode tail: one wait-loop iteration then fall through to A0 #00.
    out["stock_decode_tail"] = run_path(stock, 0xCB, 0xD7, [0x00, 0x80], init_a=0, init_x=0, init_y=0)
    out["custom_decode_tail"] = run_path(custom, 0xCB, 0xD7, [0x00, 0x80], init_a=0, init_x=0, init_y=0)
    return out


def assert_cycles(results: dict[str, int]) -> None:
    if results["stock_sync"] != EXPECTED_CYCLES["sync"]:
        raise AssertionError(f"stock sync cycles changed: {results['stock_sync']} != {EXPECTED_CYCLES['sync']}")
    if results["stock_mismatch"] != EXPECTED_CYCLES["mismatch"]:
        raise AssertionError(f"stock mismatch cycles changed: {results['stock_mismatch']} != {EXPECTED_CYCLES['mismatch']}")
    if results["stock_sync_spins"] != EXPECTED_CYCLES["sync_spins"]:
        raise AssertionError(
            f"stock sync_spins cycles changed: {results['stock_sync_spins']} != {EXPECTED_CYCLES['sync_spins']}"
        )
    if results["stock_decode_0300"] != EXPECTED_CYCLES["decode_0300"]:
        raise AssertionError(
            f"stock decode_0300 cycles changed: {results['stock_decode_0300']} != {EXPECTED_CYCLES['decode_0300']}"
        )
    if results["stock_decode_dst"] != EXPECTED_CYCLES["decode_dst"]:
        raise AssertionError(
            f"stock decode_dst cycles changed: {results['stock_decode_dst']} != {EXPECTED_CYCLES['decode_dst']}"
        )
    if results["stock_pack"] != EXPECTED_CYCLES["pack"]:
        raise AssertionError(f"stock pack cycles changed: {results['stock_pack']} != {EXPECTED_CYCLES['pack']}")
    if results["stock_decode_tail"] != EXPECTED_CYCLES["decode_tail"]:
        raise AssertionError(
            f"stock decode_tail cycles changed: {results['stock_decode_tail']} != {EXPECTED_CYCLES['decode_tail']}"
        )

    if results["custom_sync"] != results["stock_sync"]:
        raise AssertionError(f"custom sync cycles differ: custom={results['custom_sync']}, stock={results['stock_sync']}")
    if results["custom_mismatch"] != results["stock_mismatch"]:
        raise AssertionError(
            f"custom mismatch cycles differ: custom={results['custom_mismatch']}, stock={results['stock_mismatch']}"
        )
    if results["custom_sync_spins"] != results["stock_sync_spins"]:
        raise AssertionError(
            f"custom sync_spins cycles differ: custom={results['custom_sync_spins']}, stock={results['stock_sync_spins']}"
        )
    if results["custom_decode_0300"] != results["stock_decode_0300"]:
        raise AssertionError(
            f"custom decode_0300 cycles differ: custom={results['custom_decode_0300']}, "
            f"stock={results['stock_decode_0300']}"
        )
    if results["custom_decode_dst"] != results["stock_decode_dst"]:
        raise AssertionError(
            f"custom decode_dst cycles differ: custom={results['custom_decode_dst']}, stock={results['stock_decode_dst']}"
        )
    if results["custom_pack"] != results["stock_pack"]:
        raise AssertionError(f"custom pack cycles differ: custom={results['custom_pack']}, stock={results['stock_pack']}")
    if results["custom_decode_tail"] != results["stock_decode_tail"]:
        raise AssertionError(
            f"custom decode_tail cycles differ: custom={results['custom_decode_tail']}, stock={results['stock_decode_tail']}"
        )


def verify(stock: bytes, custom: bytes) -> dict[str, int]:
    assert_pinned_bytes(stock, "stock")
    assert_pinned_bytes(custom, "custom")
    assert_prodos_fingerprint(stock, "stock")
    assert_prodos_fingerprint(custom, "custom")
    assert_entry_point_signatures(stock, "stock")
    assert_entry_point_signatures(custom, "custom")
    results = measure_cycles(stock, custom)
    assert_cycles(results)
    return results


def main() -> None:
    stock = STOCK_ROM.read_bytes()
    custom = CUSTOM_ROM.read_bytes()
    results = verify(stock, custom)

    print("PASS: signature bytes and critical path cycle counts verified")
    print(
        "  "
        f"sync={results['custom_sync']}, mismatch={results['custom_mismatch']}, "
        f"sync_spins={results['custom_sync_spins']}, "
        f"decode_0300={results['custom_decode_0300']}, decode_dst={results['custom_decode_dst']}, "
        f"decode_tail={results['custom_decode_tail']}, pack={results['custom_pack']}"
    )


if __name__ == "__main__":
    main()

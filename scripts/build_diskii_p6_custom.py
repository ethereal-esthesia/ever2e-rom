#!/usr/bin/env python3
"""Build a Disk II P6 substitute ROM and matching custom boot-test disk."""

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
    return build_standard_loader_code()


def build_custom_stream_loader_code() -> bytes:
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


def build_standard_loader_code() -> bytes:
    asm = Assembler(ROM_BASE)

    asm.emit(
        0xA2, 0x20,       # LDX #$20
        0xA0, 0x00,       # LDY #$00
        0xA2, 0x03,       # LDX #$03
    )
    asm.label("table_loop")
    asm.emit(
        0x86, 0x3C,       # STX $3C
        0x8A,             # TXA
        0x0A,             # ASL A
        0x24, 0x3C,       # BIT $3C
    )
    asm.branch(0xF0, "table_next")  # BEQ table_next
    asm.emit(
        0x05, 0x3C,       # ORA $3C
        0x49, 0xFF,       # EOR #$FF
        0x29, 0x7E,       # AND #$7E
    )
    asm.label("table_shift")
    asm.branch(0xB0, "table_next")  # BCS table_next
    asm.emit(0x4A)                   # LSR A
    asm.branch(0xD0, "table_shift")  # BNE table_shift
    asm.emit(
        0x98,                         # TYA
        0x9D, 0x56, 0x03,             # STA $0356,X
        0xC8,                         # INY
    )
    asm.label("table_next")
    asm.emit(0xE8)                    # INX
    asm.branch(0x10, "table_loop")    # BPL table_loop

    asm.emit_abs(0x20, 0xFF58)        # JSR $FF58
    asm.emit(
        0xA2, 0x60,                   # LDX #$60, P6 controller base
        0x86, 0x2B,                   # STX $2B
        0xEA, 0xEA, 0xEA, 0xEA,       # keep downstream disk timing aligned
        0xEA, 0xEA, 0xEA,
        0xBD, 0x8E, 0xC0,             # LDA $C08E,X
        0xBD, 0x8C, 0xC0,             # LDA $C08C,X
        0xBD, 0x8A, 0xC0,             # LDA $C08A,X
        0xBD, 0x89, 0xC0,             # LDA $C089,X
        0xA0, 0x50,                   # LDY #$50
    )
    asm.label("recalibrate_loop")
    asm.emit(
        0xBD, 0x80, 0xC0,             # LDA $C080,X
        0x98,                         # TYA
        0x29, 0x03,                   # AND #$03
        0x0A,                         # ASL A
        0x05, 0x2B,                   # ORA $2B
        0xAA,                         # TAX
        0xBD, 0x81, 0xC0,             # LDA $C081,X
        0xA9, 0x56,                   # LDA #$56
        0x20, 0xA8, 0xFC,             # JSR $FCA8
        0x88,                         # DEY
    )
    asm.branch(0x10, "recalibrate_loop")  # BPL recalibrate_loop

    asm.emit(
        0x85, 0x26,                   # STA $26
        0x85, 0x3D,                   # STA $3D
        0x85, 0x41,                   # STA $41
        0xA9, 0x08,                   # LDA #$08
        0x85, 0x27,                   # STA $27
    )
    asm.label("next_sector")
    asm.emit(0x18)                    # CLC
    asm.label("search_prologue")
    asm.emit(0x08)                    # PHP
    asm.label("read_d5")
    asm.emit(0xBD, 0x8C, 0xC0)        # LDA $C08C,X
    asm.branch(0x10, "read_d5")       # BPL read_d5
    asm.label("match_d5")
    asm.emit(0x49, 0xD5)              # EOR #$D5
    asm.branch(0xD0, "read_d5")       # BNE read_d5
    asm.label("read_aa")
    asm.emit(0xBD, 0x8C, 0xC0)        # LDA $C08C,X
    asm.branch(0x10, "read_aa")       # BPL read_aa
    asm.emit(0xC9, 0xAA)              # CMP #$AA
    asm.branch(0xD0, "match_d5")      # BNE match_d5
    asm.emit(0xEA)                    # NOP
    asm.label("read_kind")
    asm.emit(0xBD, 0x8C, 0xC0)        # LDA $C08C,X
    asm.branch(0x10, "read_kind")     # BPL read_kind
    asm.emit(0xC9, 0x96)              # CMP #$96
    asm.branch(0xF0, "address_field") # BEQ address_field
    asm.emit(0x28)                    # PLP
    asm.branch(0x90, "next_sector")   # BCC next_sector
    asm.emit(0x49, 0xAD)              # EOR #$AD
    asm.branch(0xF0, "data_field")    # BEQ data_field
    asm.branch(0xD0, "next_sector")   # BNE next_sector

    asm.label("address_field")
    asm.emit(0xA0, 0x03)              # LDY #$03
    asm.label("address_decode_loop")
    asm.emit(0x85, 0x40)              # STA $40
    asm.label("address_read_hi")
    asm.emit(0xBD, 0x8C, 0xC0)        # LDA $C08C,X
    asm.branch(0x10, "address_read_hi")
    asm.emit(
        0x2A,                         # ROL A
        0x85, 0x3C,                   # STA $3C
    )
    asm.label("address_read_lo")
    asm.emit(0xBD, 0x8C, 0xC0)        # LDA $C08C,X
    asm.branch(0x10, "address_read_lo")
    asm.emit(
        0x25, 0x3C,                   # AND $3C
        0x88,                         # DEY
    )
    asm.branch(0xD0, "address_decode_loop")
    asm.emit(
        0x28,                         # PLP
        0xC5, 0x3D,                   # CMP $3D
    )
    asm.branch(0xD0, "next_sector")
    asm.emit(
        0xA5, 0x40,                   # LDA $40
        0xC5, 0x41,                   # CMP $41
    )
    asm.branch(0xD0, "next_sector")
    asm.branch(0xB0, "search_prologue")

    asm.label("data_field")
    asm.emit(0xA0, 0x56)              # LDY #$56
    asm.label("decode_aux")
    asm.emit(0x84, 0x3C)              # STY $3C
    asm.label("read_aux")
    asm.emit(0xBC, 0x8C, 0xC0)        # LDY $C08C,X
    asm.branch(0x10, "read_aux")
    asm.emit(
        0x59, 0xD6, 0x02,             # EOR $02D6,Y
        0xA4, 0x3C,                   # LDY $3C
        0x88,                         # DEY
        0x99, 0x00, 0x03,             # STA $0300,Y
    )
    asm.branch(0xD0, "decode_aux")
    asm.label("decode_main")
    asm.emit(0x84, 0x3C)              # STY $3C
    asm.label("read_main")
    asm.emit(0xBC, 0x8C, 0xC0)        # LDY $C08C,X
    asm.branch(0x10, "read_main")
    asm.emit(
        0x59, 0xD6, 0x02,             # EOR $02D6,Y
        0xA4, 0x3C,                   # LDY $3C
        0x91, 0x26,                   # STA ($26),Y
        0xC8,                         # INY
    )
    asm.branch(0xD0, "decode_main")
    asm.label("read_checksum")
    asm.emit(0xBC, 0x8C, 0xC0)        # LDY $C08C,X
    asm.branch(0x10, "read_checksum")
    asm.emit(0x59, 0xD6, 0x02)        # EOR $02D6,Y
    asm.label("checksum_done")
    asm.branch(0xD0, "next_sector")
    asm.emit(
        0xA0, 0x00,                   # LDY #$00
    )
    asm.label("denib_outer")
    asm.emit(0xA2, 0x56)              # LDX #$56
    asm.label("denib_inner")
    asm.emit(
        0xCA,                         # DEX
    )
    asm.branch(0x30, "denib_outer")   # BMI denib_outer
    asm.emit(
        0xB1, 0x26,                   # LDA ($26),Y
        0x5E, 0x00, 0x03,             # LSR $0300,X
        0x2A,                         # ROL A
        0x5E, 0x00, 0x03,             # LSR $0300,X
        0x2A,                         # ROL A
        0x91, 0x26,                   # STA ($26),Y
        0xC8,                         # INY
    )
    asm.branch(0xD0, "denib_inner")
    asm.emit(
        0xE6, 0x27,                   # INC $27
        0xE6, 0x3D,                   # INC $3D
        0xA5, 0x3D,                   # LDA $3D
        0xCD, 0x00, 0x08,             # CMP $0800
        0xA6, 0x2B,                   # LDX $2B
    )
    asm.branch(0x90, "checksum_done") # BCC checksum_done
    asm.emit_abs(0x4C, PAYLOAD_ADDR + 1)  # JMP $0801

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


def build_custom_stream_bytes() -> bytearray:
    code = build_custom_stream_loader_code()
    if len(code) >= ROM_SIZE:
        raise ValueError(f"custom stream loader is too large for P6 ROM: {len(code)} bytes")

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
        help="Output generated custom stream boot-test NIB path",
    )
    parser.add_argument(
        "--custom-stream-out",
        default="ROMS/DISKII_P6_BOOT_TEST.rom",
        help="Output boot-test ROM path for the generated custom stream NIB",
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

    stream_path = Path(args.custom_stream_out)
    stream_path.parent.mkdir(parents=True, exist_ok=True)
    stream = build_custom_stream_bytes()
    stream_path.write_bytes(stream)

    if args.stock_out:
        stock_path = Path(args.stock_out)
        if stock_path.exists():
            stock_path.unlink()

    print(f"Wrote {out_path} ({len(custom)} bytes)")
    print(f"Wrote {stream_path} ({len(stream)} bytes)")
    print(f"Wrote {boot_path} ({len(boot)} bytes)")
    print(
        "Pinned signature bytes:",
        ", ".join(f"${k:02X}=${v:02X}" for k, v in COMPAT_SIGNATURE.items()),
    )


if __name__ == "__main__":
    main()

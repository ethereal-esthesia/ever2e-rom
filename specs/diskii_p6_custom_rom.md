# Disk II P6 Substitute ROM Notes

This repo generates a 256-byte clean-room substitute slot ROM for low-level
Disk II controller tests.

The current image is intentionally not a functional boot/read ROM. It preserves
the common 16-sector Disk II/ProDOS identification bytes used by slot-ROM
plumbing, then falls into an inert self-loop. Emulator tests that exercise the
controller directly can use this ROM without carrying the proprietary P6 image.

## Files

- `scripts/build_diskii_p6_custom.py`
- `scripts/test_diskii_p6_rom.py`
- `tests/test_diskii_p6_entrypoints.py`

Generated local artifact:

- `ROMS/DISKII_P6_CUSTOM.rom`

`ROMS/` is a generated build-output folder and is ignored by git.

## Build

```bash
make p6-roms
```

The build also removes a stale `ROMS/DISKII_P6_STOCK.rom` if one exists.

## Substitute ROM Layout

- `$C600`: `LDX #$20`
- `$C602`: `LDY #$00`
- `$C604`: `LDX #$03`
- `$C606`: `STX $3C`
- `$C608`: `JMP $C608`
- `$C610`: ASCII label `EVER2E P6 TEST ROM`
- remaining bytes: `NOP` fill, except `$C6FF = $00`

Compatibility signature bytes:

- offset `$01` = `$20`
- offset `$03` = `$00`
- offset `$05` = `$03`
- offset `$07` = `$3C`
- offset `$FF` = `$00`

## Verify

```bash
make test-p6-precheck
```

This checks:

- the generated image is exactly 256 bytes
- the compatibility signature bytes are pinned
- the boot-entry bytes are pinned
- the generated label is present
- the entry stub reaches the inert `$C608` self-loop

## JVM Integration

The JVM Disk II controller supports:

```properties
machine.layout.slot.6.rom.file=ROMS/DISKII_P6_CUSTOM.rom
```

Paths are resolved relative to the `.emu` file first, then as supplied. The
JVM low-level random-write test generates the same substitute image locally and
loads it through this property.

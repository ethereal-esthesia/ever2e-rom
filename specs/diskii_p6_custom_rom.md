# Disk II P6 Substitute ROM Notes

This repo generates a 256-byte substitute slot ROM for Disk II controller tests.

The primary image is standard Disk II boot compatible: it recalibrates to track
0, reads normal 6-and-2 encoded boot sectors, and jumps to `$0801`. A separate
legacy custom-stream image is still generated for the deliberately simple
synthetic test disk.

## Files

- `scripts/build_diskii_p6_custom.py`
- `scripts/test_diskii_p6_rom.py`
- `tests/test_diskii_p6_entrypoints.py`

Generated local artifacts:

- `ROMS/DISKII_P6_CUSTOM.rom`
- `ROMS/DISKII_P6_CUSTOM_STREAM.rom`
- `ROMS/DISKII_P6_BOOT_TEST.nib`

`ROMS/` is a generated build-output folder and is ignored by git.

## Build

```bash
make p6-roms
```

The build also removes a stale `ROMS/DISKII_P6_STOCK.rom` if one exists.

## Standard Substitute ROM Layout

- `$C600`: `LDX #$20`
- `$C602`: `LDY #$00`
- `$C604`: `LDX #$03`
- `$C606`: `STX $3C`
- then: build the Disk II 6-and-2 decode table, recalibrate the drive, scan
  normal address/data prologues, decode boot sectors into `$0800+`, and jump to
  `$0801`
- remaining unused bytes: `NOP` fill, except `$C6FF = $00`

Compatibility signature bytes:

- offset `$01` = `$20`
- offset `$03` = `$00`
- offset `$05` = `$03`
- offset `$07` = `$3C`
- offset `$FF` = `$00`

## Custom Stream Boot-Test Disk

`ROMS/DISKII_P6_BOOT_TEST.nib` is a generated 35-track NIB image for
`ROMS/DISKII_P6_CUSTOM_STREAM.rom`. The stream is placed on track 34 because
the current JVM and C++ Disk II controllers initialize the head at track 34.

Track 34 layout:

- 32 bytes of `$FF` sync
- magic bytes `$E2 $B0 $B1 $B2`
- 256 generated payload bytes encoded as two high-bit nibbles per byte
- `$FF` fill

Payload encoding:

- high nibble: `$A0 | (byte >> 4)`
- low nibble: `$A0 | (byte & $0F)`

The payload is copied to `$0800`, stores `$42` at `$0400`, stores `$C8` at
`$0401`, then loops at `$080A`.

## Verify

```bash
make test-p6-precheck
```

This checks:

- the generated ROM image is exactly 256 bytes
- the compatibility signature bytes are pinned
- the boot-entry prefix is pinned
- the standard loader executable bytes are not a local stock PROM clone when one
  is present
- the generated custom boot disk has the expected magic and payload
- the custom-stream P6 loader can find the custom stream, decode the payload, copy it to
  `$0800`, and reach the payload success loop

## JVM Integration

The JVM Disk II controller supports:

```properties
machine.layout.slot.6.rom.file=ROMS/DISKII_P6_CUSTOM.rom
```

Paths are resolved relative to the `.emu` file first, then as supplied. The
JVM Disk II tests generate the same substitute image locally, load it through
this property, and include an integration test that boots the generated custom
NIB through the real CPU, slot ROM mapping, scheduler, and Disk II controller.

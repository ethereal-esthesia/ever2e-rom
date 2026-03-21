# Disk II P6 Custom ROM Notes

This repo includes validation notes for a 256-byte 5.25" Disk II
controller ROM image compatible with the classic 16-sector P6 ROM layout.

## Files
- `scripts/test_diskii_p6_rom.py`

## Compatibility Signature Bytes
Commonly checked Disk II/ProDOS signature bytes to preserve during refactors:

- offset `$01` = `$20`
- offset `$03` = `$00`
- offset `$05` = `$03`
- offset `$07` = `$3C`
- offset `$FF` = `$00` (16-sector Disk II style)

## Verify (ID Bytes + Cycle Paths)
```bash
scripts/test_diskii_p6_rom.py
```

This test checks:
- pinned compatibility bytes (`$01,$03,$05,$07,$FF`)
- cycle counts for two timing-sensitive sync/mismatch paths
- stock vs custom cycle parity on those paths

## Current Status
- Validation focuses on signature bytes plus timing-sensitive path parity.
- The boot/read loops are cycle-critical, so internal edits should be paired with these checks.

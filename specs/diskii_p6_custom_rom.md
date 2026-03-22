# Disk II P6 Custom ROM Notes

This repo includes validation notes for a 256-byte 5.25" Disk II
controller ROM image compatible with the classic 16-sector P6 ROM layout.

## Files
- `scripts/test_diskii_p6_rom.py`
- `scripts/build_diskii_p6_custom.py`
- `tests/test_diskii_p6_entrypoints.py`
- `specs/roms_build_outputs.md`

Local-only artifacts (not for push):
- stock ROM output (default): `ROMS/DISKII_P6_STOCK.rom`
- custom ROM output (default): `ROMS/DISKII_P6_CUSTOM.rom`

`ROMS/` is a generated build-output folder and is ignored by git.

## Actual 5.25" ROM Image Provenance (Raw 256 Bytes)

Current stock image bytes are pinned directly in:
- `scripts/build_diskii_p6_custom.py` (`STOCK_HEX`)

Deterministic fingerprint for the 256-byte stock image:
- `SHA-256`: `de1e3e035878bab43d0af8fe38f5839c527e9548647036598ee6fe7ec74d2a7d`
- `MD5`: `2020aa1413ff77fe29353f3ee72dc295`

Quick byte sanity markers:
- First 16 bytes: `A2 20 A0 00 A2 03 86 3C 8A 0A 24 3C F0 10 05 3C`
- Last 16 bytes: `3D CD 00 08 A6 2B 90 DB 4C 01 08 00 00 00 00 00`

Build/recreate artifacts:
```bash
python3 scripts/build_diskii_p6_custom.py
```

Verify artifact hash:
```bash
shasum -a 256 ROMS/DISKII_P6_STOCK.rom
```

Note:
- `DISKII_P6_STOCK.rom` should be treated as a local reference artifact only.
- Do not commit/push stock ROM binaries; regenerate locally from `STOCK_HEX` when needed.

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
- ProDOS fingerprint bytes (`$00..$07`)
- entry-point signature bytes for boot/sync/decode/pack paths
- cycle counts for timing-sensitive paths (`sync`, `mismatch`, `sync_spins`,
  `decode_0300`, `decode_dst`, `pack`, `decode_tail`)
- stock vs custom cycle parity on those paths

## Current Status
- Validation focuses on signature bytes, entry-point signatures, and timing parity.
- The boot/read loops are cycle-critical, so internal edits should be paired with these checks.
- Current custom image matches pinned stock bytes (`0/256 changed`, `0.0%`).

## Checklist
- [x] Pin stock 256-byte image in source (`STOCK_HEX`).
- [x] Keep ProDOS-visible signature bytes fixed (`$01,$03,$05,$07,$FF`).
- [x] Treat `$00..$07` as a pinned ProDOS/controller fingerprint.
- [x] Keep stock/custom ROM binaries local-only (not tracked in git).
- [x] Validate entry-point signatures and cycle parity via `scripts/test_diskii_p6_rom.py`.
- [x] Cover the same expectations from `unittest` (`tests/test_diskii_p6_entrypoints.py`).
- [ ] Add broader boot-path regression capture (for example, full RWTS read flow traces).

## Entry-Point Checklist
- [x] `$00` boot entry signature (`A2 20 A0 00 A2 03 86 3C`)
- [ ] `$5C` sync prologue signature
- [ ] `$5D` sync mismatch recovery signature
- [ ] `$A6` decode-to-`$0300` entry signature
- [ ] `$BA` decode-to-`($26),Y` entry signature
- [ ] `$CB` decode tail entry signature
- [ ] `$D7` bit-pack loop entry signature

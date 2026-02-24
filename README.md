# Custom Apple IIe Platinum ROM Spec (65C02) v0.1

## 1. Target
- Machine: Apple IIe Platinum-compatible
- CPU: `65C02` only (use 65C02 instructions)
- ROM image: `16 KB` motherboard ROM (`$C000-$FFFF` mapping behavior compatible with IIe)
- Features: built-in `Monitor`, `BASIC`, and ROM-resident `DOS bootstrap`

## 2. Important constraint
- Full Applesoft + full DOS 3.3 + Monitor do not all fit comfortably as full implementations in 16 KB.
- Recommended architecture:
  - Keep Monitor + BASIC resident in ROM.
  - Keep DOS in ROM as a **bootstrap/command stub + RWTS hooks**, then load full DOS components into RAM from disk.
- Memory-use policy:
  - Prefer using expanded/banked RAM (language-card and auxiliary RAM) for system-resident code/data.
  - Preserve regular/main RAM as much as possible for user programs and user data.
  - Keep ROM mapped at reset/boot, then flip to RAM overlays where safe for performance and feature growth.

## 3. Memory layout (proposed)
- `$D000-$E7FF`: BASIC core/interpreter
- `$E800-$EFFF`: DOS command stub + disk API trampoline
- `$F000-$F7FF`: Monitor core (disassembler, examine/store, mini-assembler)
- `$F800-$FFF3`: reset/autostart, common I/O glue, jump table
- `$FFF4-$FFF9`: reserved
- `$FFFA-$FFFF`: vectors
  - `NMI` at `$FFFA/$FFFB`
  - `RESET` at `$FFFC/$FFFD`
  - `IRQ/BRK` at `$FFFE/$FFFF`

## 4. Hardware address map (IIe/Platinum baseline)
- Main memory:
  - `$0000-$00FF`: zero page
  - `$0100-$01FF`: hardware stack
  - `$0200-$03FF`: general RAM
  - `$0400-$07FF`: text/lores page 1
  - `$0800-$0BFF`: text/lores page 2
  - `$0C00-$1FFF`: general RAM
  - `$2000-$3FFF`: hi-res page 1
  - `$4000-$5FFF`: hi-res page 2
  - `$6000-$BFFF`: general RAM
- I/O and firmware windows:
  - `$C000-$C0FF`: soft-switch/I/O space
  - `$C100-$C2FF`, `$C400-$C7FF`: slot ROM (`$CnXX`) or internal ROM (via `INTCXROM`)
  - `$C300-$C3FF`: slot 3/internal ROM control window (`SLOTC3ROM`/`INTC8ROM` interactions)
  - `$C800-$CFFF`: expansion ROM window or internal ROM (`INTC8ROM`/`INTCXROM` dependent)
  - `$D000-$FFFF`: upper ROM / language-card banked RAM region
- Core soft switches and status (relevant to ROM work):
  - `$C000`: keyboard data / key strobe
  - `$C010`: clear/read keyboard strobe
  - `$C001`: set `80STORE`
  - `$C002/$C003`: clear/set `RAMRD`
  - `$C004/$C005`: clear/set `RAMWRT`
  - `$C006/$C007`: clear/set `INTCXROM`
  - `$C008/$C009`: clear/set `ALTZP`
  - `$C00A/$C00B`: clear/set `SLOTC3ROM`
  - `$C00C/$C00D`: clear/set `80COL`
  - `$C00E/$C00F`: clear/set `ALTCHARSET`
  - `$C011-$C01F`: switch status reads (`BANK1`, `HRAMRD`, `RAMRD`, `RAMWRT`, `INTCXROM`, `ALTZP`, `SLOTC3ROM`, `80STORE`, `VBL`, `TEXT`, `MIXED`, `PAGE2`, `HIRES`, `ALTCHARSET`, `80COL`)
  - `$C019`: vertical blank status (`VBL`) in bit 7
  - `$C030`: speaker toggle
  - `$C050/$C051`: graphics/text mode
  - `$C052/$C053`: mixed/full mode
  - `$C054/$C055`: page2/page1 select
  - `$C056/$C057`: lores/hires select
  - `$C058-$C05F`: annunciators 0-3 clear/set
  - `$C061/$C069`: open-apple / PB0
  - `$C062/$C06A`: closed-apple (option) / PB1
  - `$C063/$C06B`: shift / PB2
  - `$C070`: paddle timer strobe
  - `$C080-$C08F`: language-card/banked-RAM control soft switches
  - `$CFFF`: clear `INTC8ROM` latch (returns expansion window routing to default behavior)
- CPU vectors (valid for 6502 and 65C02):
  - `$FFFA/$FFFB`: `NMI`
  - `$FFFC/$FFFD`: `RESET`
  - `$FFFE/$FFFF`: `IRQ/BRK`

## 5. Boot behavior
- Cold reset:
  - Init hardware state, soft switches, stack, zero page workspace
  - Print banner (`APPLE IIE PLATINUM 65C02`)
  - Attempt slot-6 boot first (DOS disk)
  - If boot fails, enter BASIC prompt (`]`)
- Warm reset (`Ctrl-Reset`):
  - Do not clear full RAM
  - Return to Monitor or BASIC based on warm-start flag

## 6. User-visible entry points
- `RESET` -> autostart sequence
- Monitor entry command -> Monitor prompt (`*`)
- BASIC command `DOS` -> DOS stub command mode
- Monitor command to jump back to BASIC warm entry

## 7. DOS scope (ROM)
- Provide:
  - Boot sector loader
  - Minimal sector read/write API
  - `CATALOG`, `LOAD`, `SAVE`, `RUN` front-end stubs
- Out of ROM scope:
  - Full DOS command processor complexity (load into RAM modules)

## 8. Compatibility requirements
- Preserve Apple IIe-style vectors and reset semantics
- Preserve common Monitor workflows (enter monitor, memory examine/store, run)
- Keep language-card/banked RAM interactions IIe-compatible
- Keep slot ROM behavior compatible with standard Disk II boot expectations
- Preserve user-visible memory headroom by default (system should consume expanded RAM first)

## 9. 65C02 rules
- Allowed opcodes: `BRA`, `STZ`, `PHX/PLX`, `PHY/PLY`, `TSB/TRB`, etc.
- Decimal mode handling must be explicit and reset-safe
- IRQ/NMI prologue/epilogue must preserve 65C02 behavior

## 10. Acceptance tests
- Boot with no disk -> BASIC prompt appears
- `PRINT 2+2` works in BASIC
- Monitor entry command enters Monitor
- With DOS disk inserted, auto-boot to DOS-loaded environment
- DOS stub commands can `CATALOG` and `LOAD` a BASIC file
- Warm reset returns cleanly without corrupting low memory state

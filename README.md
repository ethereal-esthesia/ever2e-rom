# Custom Apple IIe Platinum ROM Spec (6502) v0.1

## Planned ROM Families
This repo is expected to produce multiple Apple IIe ROM variants over time:

- a monitor injection ROM
- an OS ROM
- additional test ROMs

## 1. Target
- Machine: Apple IIe Platinum-compatible
- CPU: `6502` only (use 6502 instructions)
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
| Range | Purpose |
|---|---|
| `$D000-$E7FF` | BASIC core/interpreter |
| `$E800-$EFFF` | DOS command stub + disk API trampoline |
| `$F000-$F7FF` | Monitor core (disassembler, examine/store, mini-assembler) |
| `$F800-$FFF3` | reset/autostart, common I/O glue, jump table |
| `$FFF4-$FFF9` | reserved |
| `$FFFA-$FFFF` | vectors (see table below) |

| Vector | Address bytes |
|---|---|
| `NMI` | `$FFFA/$FFFB` |
| `RESET` | `$FFFC/$FFFD` |
| `IRQ/BRK` | `$FFFE/$FFFF` |

## 4. Hardware address map (IIe/Platinum baseline)
| Main memory range | Meaning |
|---|---|
| `$0000-$00FF` | zero page |
| `$0100-$01FF` | hardware stack |
| `$0200-$03FF` | general RAM |
| `$0400-$07FF` | text/lores page 1 |
| `$0800-$0BFF` | text/lores page 2 |
| `$0C00-$1FFF` | general RAM |
| `$2000-$3FFF` | hi-res page 1 |
| `$4000-$5FFF` | hi-res page 2 |
| `$6000-$BFFF` | general RAM |

| I/O / firmware window | Meaning |
|---|---|
| `$C000-$C0FF` | soft-switch/I/O space |
| `$C100-$C2FF`, `$C400-$C7FF` | slot ROM (`$CnXX`) or internal ROM (via `INTCXROM`) |
| `$C300-$C3FF` | slot 3/internal ROM control window (`SLOTC3ROM`/`INTC8ROM` interactions) |
| `$C800-$CFFF` | expansion ROM window or internal ROM (`INTC8ROM`/`INTCXROM` dependent) |
| `$D000-$FFFF` | upper ROM / language-card banked RAM region |

| Soft switch / status | Function |
|---|---|
| `$C000` | keyboard data / key strobe |
| `$C010` | clear/read keyboard strobe |
| `$C001` | set `80STORE` |
| `$C002/$C003` | clear/set `RAMRD` |
| `$C004/$C005` | clear/set `RAMWRT` |
| `$C006/$C007` | clear/set `INTCXROM` |
| `$C008/$C009` | clear/set `ALTZP` |
| `$C00A/$C00B` | clear/set `SLOTC3ROM` |
| `$C00C/$C00D` | clear/set `80COL` |
| `$C00E/$C00F` | clear/set `ALTCHARSET` |
| `$C011-$C01F` | switch status reads (`BANK1`, `HRAMRD`, `RAMRD`, `RAMWRT`, `INTCXROM`, `ALTZP`, `SLOTC3ROM`, `80STORE`, `VBL`, `TEXT`, `MIXED`, `PAGE2`, `HIRES`, `ALTCHARSET`, `80COL`) |
| `$C019` | vertical blank status (`VBL`) in bit 7 |
| `$C030` | speaker toggle |
| `$C050/$C051` | graphics/text mode |
| `$C052/$C053` | mixed/full mode |
| `$C054/$C055` | page2/page1 select |
| `$C056/$C057` | lores/hires select |
| `$C058-$C05F` | annunciators 0-3 clear/set |
| `$C061/$C069` | open-apple / PB0 |
| `$C062/$C06A` | closed-apple (option) / PB1 |
| `$C063/$C06B` | shift / PB2 |
| `$C070` | paddle timer strobe |
| `$C080-$C08F` | language-card/banked-RAM control soft switches |
| `$CFFF` | clear `INTC8ROM` latch (returns expansion window routing to default behavior) |

| CPU vector (6502 + 65C02) | Address bytes |
|---|---|
| `NMI` | `$FFFA/$FFFB` |
| `RESET` | `$FFFC/$FFFD` |
| `IRQ/BRK` | `$FFFE/$FFFF` |

### Switchable Spaces Matrix
| Space | Address range | What switches | Primary soft-switches | Control addresses | Notes |
|---|---|---|---|---|---|
| Zero page + stack | `$0000-$01FF` | Main vs auxiliary bank | `ALTZP` | `C008` (clear) / `C009` (set) | Switches both ZP and stack together |
| General RAM (most lower memory) | `$0200-$03FF`, `$0800-$1FFF`, `$4000-$BFFF` | Main vs auxiliary read/write | `RAMRD`, `RAMWRT` | `C002/C003` (`RAMRD`), `C004/C005` (`RAMWRT`) | Read and write routing are independent |
| Text/Lores page region | `$0400-$07FF` | With `80STORE` on, `PAGE2` selects display page bank; otherwise `RAMRD/RAMWRT` rules | `80STORE`, `PAGE2` (+ `RAMRD/RAMWRT` fallback) | `C001` (set `80STORE`), `C054/C055` (`PAGE2`) | Special-cased by `80STORE` behavior |
| Hires page region | `$2000-$3FFF` | With `80STORE` and `HIRES` on, `PAGE2` selects bank; otherwise `RAMRD/RAMWRT` rules | `80STORE`, `HIRES`, `PAGE2` (+ `RAMRD/RAMWRT` fallback) | `C001` (`80STORE`), `C056/C057` (`HIRES`), `C054/C055` (`PAGE2`) | Special-cased by `80STORE+HIRES` behavior |
| Slot ROM window | `$C100-$C2FF`, `$C400-$C7FF` | Internal ROM vs slot card ROM | `INTCXROM` | `C006` (clear) / `C007` (set) | Set `INTCXROM` to force internal ROM in `CNXX` |
| Slot 3 control window | `$C300-$C3FF` | Internal ROM vs slot 3 ROM plus C8 latch behavior | `INTCXROM`, `SLOTC3ROM`, `INTC8ROM` latch | `C00A/C00B` (`SLOTC3ROM`), `C006/C007` (`INTCXROM`) | Access to `C3XX` can set `INTC8ROM` latch when `SLOTC3ROM` is clear |
| Expansion ROM window | `$C800-$CFFF` | Internal ROM vs expansion/card ROM | `INTC8ROM` latch + `INTCXROM` | `CFFF` clears `INTC8ROM`; latch set via `C3XX` path | Routing depends on C8 latch state |
| Upper firmware banked area | `$D000-$DFFF` | ROM vs banked RAM and bank select | `HRAMRD`, `HRAMWRT`, `BANK1` (+ `ALTZP` backing) | `C080-C08F` language-card controls | Classic language-card bank-switching region |
| Upper firmware fixed area | `$E000-$FFFF` | ROM vs overlay RAM | `HRAMRD`, `HRAMWRT` (+ `ALTZP` backing) | `C080-C08F` language-card controls | Not bank-split like `$D000-$DFFF` in common IIe behavior |
| Language-card control group | `$C080-$C08F` | Select bank/read/write-latch state for upper memory | `HRAMRD`, `HRAMWRT`, `BANK1`, `PREWRITE` | `C080-C08F` | Access pattern controls whether upper region is readable/writable RAM vs ROM |
| Video mode page routing | `$0400-$07FF`, `$2000-$3FFF` | Displayed page selection | `PAGE2` (+ `80STORE` and `HIRES` interactions) | `C054/C055` (`PAGE2`) | Affects which page is active for display/page-mapped accesses |

### Language Card `$C080-$C08F` Detail
| Soft-switch | Also alias | Effect on `BANK1` | Effect on `HRAMRD` | Effect on `PREWRITE` | Effect on `HRAMWRT` | Practical meaning |
|---|---|---|---|---|---|---|
| `C080` | `C084` | Clear (bank2) | Set | Clear | Clear | Read bank2 RAM in `$D000-$DFFF`; writes disabled |
| `C081` | `C085` | Clear (bank2) | Clear | Set on first access; if already set then enable write | Latches write-enable sequence | ROM read path for upper area unless `HRAMRD` is re-enabled; part of write-unlock sequence |
| `C082` | `C086` | Clear (bank2) | Clear | Clear | Clear | ROM read path; writes disabled |
| `C083` | `C087` | Clear (bank2) | Set | Set on first access; if already set then enable write | Latches write-enable sequence | Read bank2 RAM with write-unlock sequencing |
| `C088` | `C08C` | Set (bank1) | Set | Clear | Clear | Read bank1 RAM in `$D000-$DFFF`; writes disabled |
| `C089` | `C08D` | Set (bank1) | Clear | Set on first access; if already set then enable write | Latches write-enable sequence | ROM read path with bank1 selected for subsequent write-enable |
| `C08A` | `C08E` | Set (bank1) | Clear | Clear | Clear | ROM read path; writes disabled (bank1 selected) |
| `C08B` | `C08F` | Set (bank1) | Set | Set on first access; if already set then enable write | Latches write-enable sequence | Read bank1 RAM with write-unlock sequencing |

## 5. Boot behavior
- Cold reset:
  - Init hardware state, soft switches, stack, zero page workspace
  - Print banner (`Ever2e Platinum`)
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

## 11. Assembly workflow (G65SC02-friendly)
This repo is wired for a fast `ca65/ld65` loop targeting Apple IIe 16KB ROM images.

### Prereqs
- `ca65` + `ld65` from `cc65` (for example: `brew install cc65`)

### Layout
- Source: `asm/main.s`
- Linker config: `cfg/apple2e_rom16k.cfg`
- ROM output: `ROMS/EVER2E.ROM`
- Checksum output: `ROMS/checksum.txt` (MAME internal hash format)
- Build artifacts: `build/`

### Zero-page map
`asm/bank_switch.inc` is the canonical home for named zero-page allocations used by
the repo's assembly files. Treat this file as the current low-level ROM contract
for zero-page ownership and persistent bank/display state shared by the ROMs in
this repo. Code that wants to participate in the main runtime contract should use
these definitions and the bank/display helpers instead of inventing private
persistent ZP state or directly poking tracked soft-switches. The intent is to
keep this layout reusable across the ROM variants in this repo while still
staying reasonably compatible with stock-style Apple IIe ROM work where practical.

The table below reflects the shared layout currently defined there.

| Address | Symbol(s) | Purpose |
|---|---|---|
| `$00-$0F` | `ZP_SCRATCH_0` ... `ZP_SCRATCH_F` | Shared routine scratch window. Routines alias and reuse this space locally; current users include `dhgr.inc` (`$00-$05`), `romsum_f800ffff.inc` (`$00-$07`), and `cpu_guard.asm` (`$00`). |
| `$24` | `CH` | Text cursor column. This repo keeps the Apple stock name `CH`, but treat it as the shared text cursor column byte. |
| `$25` | `CV` | Text cursor row. This repo keeps the Apple stock name `CV`, but treat it as the shared text cursor row byte. |
| `$32` | `INVFLG` | Text attribute flag. This repo keeps the Apple stock name `INVFLG`; `INVFLG_NORMAL` (`$FF`), `INVFLG_FLASH` (`$7F`), and `INVFLG_INVERSE` (`$3F`) follow the Apple monitor/Applesoft convention. |
| `$FC` | `BANK_SWITCH_EXT_STATE` | Persistent extended bank state in main ZP: `INTCXROM`, `INTC8ROM`, LC prewrite latch, `AN0-AN2` |
| `$FD` | `BANK_SWITCH_COMMON_STATE` | Persistent common bank state in main ZP: LC read/bank/write plus `RAMRD`, `RAMWRT`, `ALTZP` |
| `$FE` | `DISPLAY_STATE` | Persistent display state in main ZP: `80STORE`, `PAGE2`, `HIRES`, `TEXT`, `MIXED`, `80COL`, `ALTCHARSET`, `AN3` |

### Commands
- Build ROM: `make build`
- Run in JVM emulator: `make run`
- Run with extra emulator args: `make run ARGS="--start-fullscreen --no-sound"`
- Clean intermediates: `make clean`

### Notes
- Source uses `.setcpu "65C02"` which is compatible with G65SC02 opcode usage.
- Vector table is emitted at `$FFFA-$FFFF` by the linker config.
- `asm/bank_switch.inc` is the canonical home for named zero-page allocations used by
  the repo's assembly files. `$00-$0F` is a shared scratch area, while the
  stock-style text state bytes (`CH`, `CV`, `INVFLG`) and the persistent control
  bytes at `$FC-$FE` live there so the shared helpers can keep cursor, attribute,
  display, and bank state coherent across the ROMs in this repo.
- The shared ZP layout is intended to stay reusable across ROM variants in this repo
  and, where practical, align with stock-style Apple IIe ROM conventions instead of
  drifting into a one-off private layout too early.
- In practice, that means `asm/bank_switch.inc` is also the current shared ROM ABI for:
  - which ZP bytes are scratch vs persistent
  - where tracked machine state lives
  - which helper layer owns bank/display state transitions

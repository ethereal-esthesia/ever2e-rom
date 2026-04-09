# Apple IIe Trap-Dispatch Kernel Requirements

## Goal
Build a small IIe kernel that combines:
- fast native paths for hot code (`JSR`/tight loops),
- trap-dispatched services for flexibility (`BRK` ABI),
- hot-swappable memory pages/modules.
- prioritized system stability and debuggability over strict cycle-exact behavior in non-critical paths.

This document defines what is needed to make that practical and debuggable on a 65SC02/IIe memory model.

## Kernel ABI Contract (Current)
The kernel-facing low-level ABI starts with the shared machine-state and zero-page
contract defined in `asm/bank_switch.inc`.

Current contract:
- `$00-$0F` is a shared scratch window. Kernel/runtime routines may alias and
  reuse it locally, but must not assume values there survive calls into other
  routines.
- `$FC` is persistent extended bank state in main zero page (`INTCXROM`, `INTC8ROM`, LC prewrite latch, `AN0-AN2`, `SLOTC3ROM`).
- `$FD` is persistent common bank state in main zero page.
- `$FE` is persistent display state in main zero page.
- Bank/display transitions that participate in the shared runtime contract should
  go through the helper layer so the tracked state bytes remain authoritative.
  Prefer the shared `set/reset` helpers for tracked mode changes; they fast-path
  the common `RAMRD`/`RAMWRT` flips so callers can keep using one safe API.
- Any code that bypasses the helpers is outside the shared contract and is
  responsible for reconciling hardware state before returning to kernel-managed
  flow.

This contract is intended to be reusable across the ROM families in this repo,
including monitor injection ROMs, OS ROMs, and test ROMs.

## Non-Goals (for v1)
- Preemptive multitasking
- Full POSIX-like file/process model
- Transparent safety against malicious code

## Core Architecture
1. **Resident kernel core**
- Lives in a fixed always-present region.
- Owns vectors, trap entry, dispatcher, pager, and panic path.

2. **Trap ABI (`BRK` service call)**
- `BRK` followed by a compact service descriptor (1-byte fast ID, optional extended ID).
- Handler decodes inline payload, validates length/version, dispatches through a table.
- Returns through a strict resume path (never implicit fallthrough).

3. **Hot path ABI (`JSR`)**
- Time-critical routines stay direct-callable via stable jump table.
- Shared conventions with trap ABI where possible (register and ZP clobber contract).

4. **Page/module manager**
- Loads/unloads overlays into predefined RAM windows.
- Supports atomic swap of service table entries to redirect calls at runtime.

5. **Privileged emulation tier (system processes)**
- Kernel/system background processes execute in the same emulated execution model as user/runtime processes.
- System processes use elevated capability bits and privileged opcodes/services instead of separate native-only control loops.
- Privileged operations still cross explicit ABI boundaries (no implicit direct hardware mutation paths).
- This unifies scheduler semantics, clock accounting, trace format, and checkpoint/resume behavior across system and user work.

## Memory Plan Requirements
1. **Fixed map specification**
- Define immutable regions: vectors, kernel core, dispatcher tables, panic log.
- Define swappable regions: code pages, asset/data pages, scratch buffers.

2. **Bank/switch discipline**
- Standard entry/exit policy for `RAMRD`, `RAMWRT`, `ALTZP`, `80STORE`, display flags.
- Every service declares switch side effects and restore behavior.

3. **Per-page metadata**
- Page ID, version, checksum, size, entry points, dependency bits.
- Compatibility flags for expected soft-switch baseline.

## Trap Dispatcher Requirements
1. **Encoding**
- v1: 1-byte service ID (`0x00-0x7F`) + typed payload.
- v2 extension: high-bit continuation for wider IDs.

### Integer Encoding Primitives
- `AUINT` (Arbitrary-length Unsigned INT):
  - Base-128 continuation encoding (LEB128-style).
  - Low 7 bits per byte are payload; high bit (`0x80`) means continuation.
  - Values are encoded in little-endian 7-bit groups.
  - Canonical form required (no redundant leading zero groups).
- `AINT` (Arbitrary-length signed INT):
  - Signed base-128 continuation encoding (SLEB128-style).
  - Same continuation bit convention as `AUINT`, with sign extension in the final group.
  - Canonical form required (no redundant sign-extension groups).
- `AUINT`/`AINT` are general ABI integer types and not limited to service IDs.
- Use cases: service IDs, payload lengths, coordinates, offsets, counters, and metadata fields.

2. **Validation**
- Bounds check payload length before consuming.
- Reject unknown IDs cleanly (`ERR_UNIMPL`).
- Reject incompatible version (`ERR_ABI`).

3. **Dispatch tables**
- Primary table in resident memory.
- Optional per-module extension tables.
- Atomic pointer update for hot swapping.

4. **Error instrumentation**
- Inline trap metadata bytes (error class/context tag) for diagnostics.
- Panic logger stores: PC, A/X/Y/P/SP, switch snapshot, service ID.

## Fast Path Requirements
1. **Stable call gateway**
- One jump table for performance-critical services.
- Entry points never move; implementation pointers may move.

2. **Promotion hooks**
- Optional counters to detect hot services.
- Ability to replace interpreter/trap target with native optimized target.
- JIT/native promotion should target the local host architecture so generated code paths are valid on the machine doing the execution.

## Cross-Compiler Contract
1. **Atomic op thunks**
- Cross-compiled emulated operations should lower to atomic 6502 thunks.
- Iteration 1 target: use only well-supported baseline opcodes available across 6502-family targets in scope, avoiding unstable/undocumented opcodes.
- Each thunk must have a single entry and single exit path.
- Each thunk must declare explicit input/output ABI and clobbers.
- Each thunk must restore required soft-switch baseline before return.

2. **Composable scheduling**
- Atomic thunk boundaries allow safe chaining, substitution, and runtime rebinding.
- Optimizer/JIT may reorder thunks only when ABI and side-effect contracts are preserved.

### Iteration 1 Baseline Opcode Set
- Use only documented baseline 6502 opcodes for thunk emission:
  - `ADC`, `AND`, `ASL`, `BCC`, `BCS`, `BEQ`, `BIT`, `BMI`, `BNE`, `BPL`, `BRK`, `BVC`, `BVS`
  - `CLC`, `CLD`, `CLI`, `CLV`, `CMP`, `CPX`, `CPY`
  - `DEC`, `DEX`, `DEY`, `EOR`
  - `INC`, `INX`, `INY`
  - `JMP`, `JSR`
  - `LDA`, `LDX`, `LDY`, `LSR`
  - `NOP`
  - `ORA`
  - `PHA`, `PHP`, `PLA`, `PLP`
  - `ROL`, `ROR`, `RTI`, `RTS`
  - `SBC`, `SEC`, `SED`, `SEI`
  - `STA`, `STX`, `STY`
  - `TAX`, `TAY`, `TSX`, `TXA`, `TXS`, `TYA`
- Exclude undocumented/illegal opcodes in v1.
- Optional v2 profile may add 65C02-specific documented opcodes behind a feature flag.

### Boundary and Bus Caveats (v1)
- Page-boundary effects are treated as compatibility constraints, even for documented opcodes.
- Branches (`Bxx`) incur page-cross timing penalties; cycle-exact paths must account for this.
- Indexed addressing forms that cross a page may perform extra bus reads on NMOS 6502-class behavior; avoid MMIO/soft-switch side effects on those paths.
- `JMP (indirect)` on NMOS 6502 has the `$xxFF` wraparound quirk; do not emit this form at page-end pointer locations in v1.

## Scheduler/Execution Model (v1)
1. **Cooperative tasks**
- `yield` service call.
- Run-to-yield semantics for user tasks.

2. **Budget guards for long loops**
- Back-edge check macro: decrement budget, trap/yield on zero.
- Prevents lockup and enables safe daisy-chaining between blocks.

## Daisy-Chain Audio/RAM Interleave Contract (v1)
1. **Execution order**
- Use strict alternating slices on the hot loop:
  - `audio_slice -> ram_slice -> audio_slice -> ram_slice`
- If there is no RAM work pending, run `audio_slice` only.

2. **Slice budget policy**
- Audio deadline is authoritative: speaker toggle deadlines are never skipped.
- `ram_slice` consumes only remaining budget after the current audio checkpoint.
- `ram_slice` must be resumable at any byte boundary and exit cleanly when budget is exhausted.

3. **State block (stable ABI)**
- Reserve a kernel-owned state block (prefer fixed ZP window) with at least:
  - `aud_interval` (target half-period or NCO step)
  - `aud_accum_lo/hi` (phase accumulator)
  - `aud_phase` (current speaker polarity/edge state)
  - `ram_src_lo/hi`
  - `ram_dst_lo/hi`
  - `ram_remaining_lo/hi`
  - `ram_flags` (pending/active/commit-needed)

4. **RAM swap semantics**
- RAM copy/swap service must expose:
  - `start(src,dst,len,mode)`
  - `step(max_cycles)` (or fixed-size byte step)
  - `status()` (`idle`, `active`, `done`, `error`)
- `step` is idempotent and restart-safe after yield/trap boundaries.

5. **VBL-aligned commit**
- Page-flip and visible mapping commits happen only at VBL-aware checkpoints.
- Bulk copy can run outside VBL windows, but visible pointer/switch commit must be atomic at commit point.

6. **Soft-switch discipline during interleave**
- Audio slices must not alter display/bank-routing soft-switch baseline.
- RAM slices may alter routing switches only inside guarded entry/exit wrappers and must restore baseline before returning.

7. **Extended interleave stack roadmap**
- Add bounded slices in this priority order:
  - `audio_slice`
  - `commit_slice` (VBL-gated page/display commit)
  - `input_slice`
  - `ram_slice`
  - `gc_slice`
  - `storage_slice`
  - `trace_slice`
- Add optional `mockingboard_slice` in the audio tier when a card/backend is present.
- Keep explicit per-slice cycle budgets and overrun accounting counters.
- Define a fixed-offset shared state block for resumable slice state.

8. **Variable key repeat (OS policy)**
- Add kernel-managed variable key repeat so applications get consistent behavior independent of firmware defaults.
- Policy should expose at least:
  - repeat delay (initial hold time)
  - repeat rate (repeat interval)
  - per-app override capability (with system default fallback)
- Repeat generation must run in `input_slice` on the same master scheduler timeline as audio/display commit slices.
- Invalid or out-of-range repeat settings must clamp to safe defaults rather than disabling keyboard input.

9. **Clock accuracy policy (no audio card present)**
- When no Mockingboard/audio card backend is present, maintain a kernel-owned accurate master clock and drive speaker fallback from that clock.
- Speaker fallback cadence must remain phase-accurate to the same scheduler timeline used by paging/input slices.
- Absence of external audio hardware must not relax checkpoint timing guarantees.
- Clock ownership remains in the shared emulated scheduler timeline so privileged system processes and user processes observe one canonical time base.

## CPU Accelerator Disable Policy
1. **Accelerator classes**
- `native_jit`:
  - Native/JIT-promoted blocks generated from emulated code.
- `thunk_fastpath`:
  - Precompiled 6502 thunk helpers that replace slower generic dispatch paths.
- `external_hw`:
  - Physical CPU accelerator behavior exposed by host/emulator profile (for example fast host stepping modes).

2. **Hard-off controls**
- Kernel keeps a boot/runtime control word with one disable bit per accelerator class:
  - `ACCEL_DISABLE_NATIVE_JIT`
  - `ACCEL_DISABLE_THUNK_FASTPATH`
  - `ACCEL_DISABLE_EXTERNAL_HW`
- Any disable bit forces immediate fallback at the next checkpoint boundary; no partial in-flight mode changes.

3. **Shutdown semantics per class**
- `native_jit` off:
  - stop issuing new native promotions
  - invalidate native entry table
  - rebind execution to emulated dispatcher entrypoints
- `thunk_fastpath` off:
  - stop selecting optimized thunk variants
  - route service/method calls through canonical interpreter/trap path
- `external_hw` off:
  - pin scheduler to canonical cycle profile
  - ignore host/emulator turbo multipliers for kernel timeline decisions

4. **No timing card policy**
- If no timing card (or no reliable timing source profile) is detected:
  - force `ACCEL_DISABLE_EXTERNAL_HW=1`
  - default `ACCEL_DISABLE_NATIVE_JIT=1` for deterministic bring-up
  - allow `thunk_fastpath` only when its cycle model is proven equivalent to canonical interpreter timing at checkpoint granularity
- Kernel logs a one-line reason code so traces explain why accelerators were disabled.

5. **Operator override**
- Provide monitor/debug command to toggle each class independently.
- Any manual enable request on a no-timing-card system must print a warning and require explicit confirm in debug builds.
- Release builds may allow policy file override, but must still emit a startup warning banner when deterministic timing is not guaranteed.

## BRK/Interrupt/Reset Handling
1. **BRK policy**
- `BRK` enters trap dispatcher only through kernel-owned vector.
- Dispatcher must capture PC, P, A/X/Y, SP, active slice ID, and switch snapshot before decoding payload.
- Unknown/invalid trap payload returns structured error (`ERR_UNIMPL`/`ERR_ABI`) without corrupting scheduler state.

2. **IRQ/NMI policy**
- IRQ/NMI handlers are minimal and bounded: timestamp event, set flags, and return.
- No long copy/page operations inside IRQ/NMI handlers.
- Deferred work is consumed by normal checkpoint slices (`input_slice`, `audio_slice`, `storage_slice`).

3. **Checkpoint-safe resume**
- Any BRK/IRQ/NMI boundary must be restart-safe from the last committed slice state.
- Each slice commits progress atomically (for example bytes-copied count) before yielding or trapping.
- Resume path re-applies baseline soft-switch state before re-entering slice execution.

4. **Reset policy**
- Reset performs full soft-switch baseline restore and marks all in-flight slices as aborted.
- Kernel reinitializes scheduler budgets, slice pointers, and shared state-block ownership map.
- Pending page commits are discarded unless metadata marks them commit-safe/replayable.
- Reset path writes a compact reset-reason record to panic/debug log.
- Privileged system processes resume from emulated checkpoints under the same rules as other slices; native/JIT artifacts are re-bound from checkpointed emulated state.

5. **Break-glass panic behavior**
- If trap/interrupt invariants fail, enter panic path:
  - force safe text mode baseline
  - dump state record
  - halt or return to monitor according to build flag

6. **Handled halt/restart reason types (v1)**
- `cold_reset`
  - Cause: power-on or explicit cold reboot.
  - Handling: full kernel/scheduler reinit, discard in-flight slices, restart from boot entry.
- `manual_break`
  - Cause: operator/debugger stop request.
  - Handling: checkpoint if safe, emit debug record, transfer to monitor/debug shell.
- `trap_fault`
  - Cause: invalid trap payload, ABI violation, or unrecoverable dispatcher error.
  - Handling: panic path with fault context record, optional controlled restart policy.
- `watchdog_timeout`
  - Cause: budget/heartbeat failure indicating scheduler stall.
  - Handling: panic + reset-reason log, recover from last committed checkpoint where possible.

## I/O and Device Model
1. **Driver boundary**
- Drivers invoked through service IDs, not hardcoded app calls.
- Clear async vs sync behavior contract.

2. **Display services**
- Page select, blit/copy, plot spans, mode set.
- Strict soft-switch baseline restoration after each call.

3. **Storage services**
- Block read/write primitives first.
- Higher-level FS wrappers can come later.

## Display Paging Strategy
1. **Virtual video paging via DHGR images**
- Treat DHGR page memory as a pageable image format.
- Save inactive video pages to disk as raw page images and restore on demand.
- Keep active draw/display pages resident; swap others as needed.

2. **OS-owned soft-switch authority**
- Kernel is the single writer of display and memory-routing soft switches.
- Maintain a canonical in-kernel switch-state model.
- Re-assert baseline switch state after display/page operations.
- Add optional assertions to verify switch state at service boundaries.

## Memory Pressure and App Suspension
1. **Suspend-to-disk fallback**
- If RAM pressure prevents keeping all runnable apps/modules resident, kernel may suspend background/least-recent apps.
- Eviction priority is least-used apps first (LRU/LFU policy), with foreground and system-critical tasks pinned/protected where configured.
- Suspension writes an app save-state image to disk (execution state + mapped page metadata + required capability/context info).
- Resume restores from save-state image and rebinds service/module pointers before re-entry.

2. **Deterministic suspension contract**
- Suspension/resume points must align to checkpoint-safe slice boundaries.
- Save-state format versioning and checksums are required to prevent corrupt resume paths.
- Foreground responsiveness and audio checkpoint deadlines take priority over bulk save-state I/O.
- Save-state destination should default to the disk/media associated with the app being suspended (not a single global OS disk by default).
- If that associated disk is missing/full/read-only, present a user dialog prompting for the app’s disk (or an explicit alternate target) before committing suspend state.

3. **App-switch checkpoint policy**
- On app switch, OS checkpoints the outgoing app state automatically.
- Checkpoint format should be diff-based against the app's last committed base state to reduce I/O and latency.
- Periodically emit consolidated full checkpoints to bound diff-chain length and recovery time.

4. **Disk write-lock ownership**
- During checkpoint commit, OS holds an explicit write lock for the target save-state store.
- App/runtime writes to that store are serialized through OS lock ownership (no direct concurrent writes).
- Lock state must be journaled so crash recovery can determine whether a checkpoint commit was complete, partial, or rolled back.

5. **Failure and lock-override warning**
- If the system detects stale/uncertain lock state after failure, do not silently clear lock and continue.
- Present a user warning before any lock override/unlock operation, including risk of losing last unsynced checkpoint data.
- Provide explicit recovery choices (retry recovery, force unlock, alternate media) and log selected action in panic/debug record.

6. **Recent-state recovery window target**
- Journaling plus regular periodic disk writes should support recovering full app state for approximately the previous 15 minutes before failure.
- Recovery design should prioritize deterministic replay/apply order and bounded startup recovery time.

## Debuggability Requirements
1. **Deterministic trap trace mode**
- Optional trace log: service ID, args length, cycles estimate, return code.

2. **Crash dump format**
- Single printable/monitor-friendly dump record.
- Include active page/module IDs and soft-switch states.

3. **Feature flags**
- Build/runtime toggles for tracing, safety checks, strict restore.

## Security/Robustness (pragmatic)
1. **Capability bits per module**
- Which services a module may call (optional in v1, required in v2).

2. **Checksum verification**
- Validate page/module before activation.

3. **Fail-closed activation**
- Module load failure must not corrupt current service table.

## Tooling Needed
1. **Assembler/linker support**
- Generate service ID maps and jump tables from one source of truth.

2. **Page packer**
- Build swappable page images + metadata headers.

3. **Trace/dump tools**
- Convert emulator dump/trace into readable reports.
- Validate page layout and ABI conformance.

## Recommended Build Order
1. Define memory map + soft-switch baseline contract.
2. Implement minimal `BRK` dispatcher with 8-16 core services.
3. Add stable `JSR` jump table for hot services.
4. Add page metadata and atomic service-table swapping.
5. Add trap trace + panic dump.
6. Add cooperative scheduler and budget guard macros.
7. Add optional hot-service promotion/JIT-like replacement hooks.

## Minimal v1 Service Set
- `yield`
- `panic`
- `log`
- `memcpy`
- `page_load`
- `service_bind`
- `display_mode_set`
- `display_page_set`
- `plot`
- `blit`

## Open Decisions
- Final trap payload encoding (fixed vs TLV-like)
- Where to place persistent kernel tables in current ROM/RAM layout
- Whether module code can directly touch soft switches or must call wrappers only
- Exact ABI for register preservation and ZP scratch ownership

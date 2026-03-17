# Apple IIe Trap-Dispatch Kernel Requirements

## Goal
Build a small IIe kernel that combines:
- fast native paths for hot code (`JSR`/tight loops),
- trap-dispatched services for flexibility (`BRK` ABI),
- hot-swappable memory pages/modules.
- prioritized system stability and debuggability over strict cycle-exact behavior in non-critical paths.

This document defines what is needed to make that practical and debuggable on a 65SC02/IIe memory model.

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

## Scheduler/Execution Model (v1)
1. **Cooperative tasks**
- `yield` service call.
- Run-to-yield semantics for user tasks.

2. **Budget guards for long loops**
- Back-edge check macro: decrement budget, trap/yield on zero.
- Prevents lockup and enables safe daisy-chaining between blocks.

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

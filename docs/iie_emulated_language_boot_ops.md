# Apple IIe Emulated Language Boot Operations

## Purpose
Define a practical operation set to bootstrap an emulated language runtime on top of the trap-dispatch kernel.

## Boot Phases
1. Platform init
- reset soft-switch baseline
- set display/text defaults
- initialize stack/ZP ownership map

2. Kernel handshake
- verify kernel ABI version
- verify feature bits
- bind required services

3. Runtime image load
- allocate/load code pages
- allocate/load constant/data pages
- verify checksums

4. Runtime activation
- initialize heap arenas
- initialize symbol table handles
- initialize module table
- run language `init`

5. REPL/program entry
- start REPL loop or call program entrypoint
- attach error trap + panic reporter

## Suggested Core Operations

### System and Control
- `op.nop`
- `op.yield`
- `op.halt`
- `op.panic`
- `op.assert`
- `op.get_ticks`
- `op.get_caps`

### Module and Service Wiring
- `op.service_bind`
- `op.service_unbind`
- `op.module_load`
- `op.module_unload`
- `op.module_query`

### Memory and Paging
- `op.memcpy`
- `op.memset`
- `op.memcmp`
- `op.page_map`
- `op.page_unmap`
- `op.page_copy`
- `op.page_crc`

### Integer/Value Primitives
- `op.auint_decode`
- `op.auint_encode`
- `op.aint_decode`
- `op.aint_encode`
- `op.add`
- `op.sub`
- `op.mul`
- `op.div`
- `op.mod`
- `op.shl`
- `op.shr`
- `op.and`
- `op.or`
- `op.xor`
- `op.not`
- `op.cmp`

### Control Flow
- `op.call`
- `op.ret`
- `op.jmp`
- `op.jz`
- `op.jnz`
- `op.jlt`
- `op.jgt`
- `op.trap`

### Stack and Frames
- `op.push`
- `op.pop`
- `op.frame_enter`
- `op.frame_leave`
- `op.local_get`
- `op.local_set`

### Heap and Objects
- `op.heap_alloc`
- `op.heap_free`
- `op.heap_realloc`
- `op.obj_new`
- `op.obj_get`
- `op.obj_set`
- `op.str_new`
- `op.str_concat`

### I/O and Host Bridge
- `op.io_read`
- `op.io_write`
- `op.console_putc`
- `op.console_getc`
- `op.file_open`
- `op.file_read`
- `op.file_write`
- `op.file_close`

### Audio (Phase 1 Stub)
- `op.audio_voice_set(freq, volume)`
- `op.audio_stop()`
- Backing device in phase 1 is Apple speaker only (Mockingboard-compatible API stub shape).
- Simultaneous voices in phase 1: `1` maximum.
- Volume range: `0-15`, where `0` is mute and `1-15` map to full volume behavior.
- Pitch generation should use a phase-accumulator (NCO-style) scheduler instead of fixed-cycle gaps.
- At each daisy-chain checkpoint: add `freq_step` to an accumulator and toggle speaker on overflow.
- This permits fractional average periods by alternating nearby integer intervals (`N`/`N+1`), producing smoother pitch gradients.
- Keep a hard upper checkpoint budget to limit jitter and preserve predictable audio cadence.

### Audio + Page Work Interleave Profile (Phase 1)
- Runtime hot loop should alternate checkpoints as:
  - `audio_slice -> page_slice -> audio_slice -> page_slice`
- Audio checkpoint deadline is authoritative.
- `page_slice` (copy/map prep) may consume only remaining budget after the audio checkpoint.
- Page/display commit is VBL-aware and atomic; bulk page copy can be incremental between commits.
- Services should expose incremental paging operations (`start`, `step`, `status`) so long copies do not block audio cadence.

### Interleave Stack Expansion (Future Phases)
- Expand checkpoint stack to include:
  - `audio_slice`
  - `commit_slice` (VBL-gated display/page commit)
  - `input_slice`
  - `ram_slice`
  - `gc_slice`
  - `storage_slice`
  - `trace_slice`
- Add `mockingboard_slice` in the audio tier when card/backend support is enabled.
- Enforce bounded per-slice budgets and overrun counters.
- Standardize fixed-offset shared state bytes for resumable checkpoint state.

### Display/Graphics
- `op.display_mode_set`
- `op.display_page_set`
- `op.plot`
- `op.blit`
- `op.clear`

### Diagnostics
- `op.log`
- `op.trace_on`
- `op.trace_off`
- `op.dump_state`
- `op.dump_pages`

## Minimum Boot Subset (Implement First)
- `op.get_caps`
- `op.service_bind`
- `op.module_load`
- `op.page_map`
- `op.memcpy`
- `op.heap_alloc`
- `op.call`
- `op.ret`
- `op.console_putc`
- `op.console_getc`
- `op.log`
- `op.panic`

## Encoding Guidance
- Use `AUINT` for IDs, lengths, offsets, and sizes.
- Use `AINT` for signed arithmetic and relative jumps.
- Keep instruction stream canonical (no non-minimal integer encodings).

## Runtime Safety Rules
- Every operation declares clobbers (registers/ZP/switches).
- Any operation touching soft-switches must restore baseline before return.
- Long-running operations must periodically `yield` or trap on budget exhaustion.

## Privileged Emulation Tier (System Processes)
- System/background kernel processes run in the same emulated execution model as language/runtime processes.
- Elevated behavior is granted through privileged capability bits and privileged opcodes/services, not by bypassing the emulated scheduler.
- This keeps one canonical scheduler clock for both system and runtime tasks.
- Reset/resume follows one checkpoint model: resume from emulated process/slice checkpoints, then rebind any native/JIT accelerators from that state.

## Trap/Interrupt/Reset Operational Rules
- BRK/trap entry records enough context to resume or report fault without losing checkpoint progress.
- IRQ/NMI handlers stay minimal and defer substantive work to normal checkpoint slices.
- Slice progress updates are atomic so resume after trap/interrupt is deterministic.
- Reset aborts in-flight slice work, restores baseline soft-switches, and reinitializes scheduler state before runtime re-entry.

## Handled Halt/Restart Reasons (v1)
- `cold_reset`: full restart from boot path.
- `manual_break`: operator/debug stop to monitor/debug shell.
- `trap_fault`: panic/fault path from unrecoverable trap/ABI error.
- `watchdog_timeout`: scheduler stall recovery path from watchdog failure.

## Sandboxing Model (Phase 1)
1. **Local-address-only execution**
- Emulated code can address only virtual local memory regions managed by the runtime.
- Emulated code cannot directly access hardware soft switches, ROM windows, or raw main/aux bank mapping.
- All hardware-touching behavior must go through kernel service calls.

2. **Address translation boundary**
- Kernel/runtime maps virtual pages to physical backing pages (main/aux/disk-backed) behind the ABI boundary.
- Emulated code remains unaware of physical address layout and bank-switch implementation details.

3. **Lazy library method loading**
- Method dispatch uses service/module tables with unresolved entries allowed at startup.
- First call to an unresolved method traps to loader/binder logic.
- Loader pages in required module code/data, validates metadata, and patches dispatch target atomically.
- Subsequent calls use the bound target directly.

4. **Display scope for phase 1**
- Output mode is constrained to text 40-column only.
- No graphics mode or DHGR APIs are exposed to emulated code in phase 1.
- Provide a text page-flip commit model: write updates to a non-visible text page, then issue a commit/flip call to make changes visible atomically.

## Suggested Milestones
1. Bring-up: interpreter + minimum boot subset
2. Language core: functions, locals, arithmetic, branching
3. Heap/object model + strings
4. Module paging + service rebinding
5. Native promotion of hot ops (architecture-local JIT path)

## Phase Constraints
- Phase 1: no JIT/native promotion; execution is interpreter/trap-dispatch only.
- Future phases with JIT/native promotion must enforce daisy-chain/yield checkpoints frequently enough to observe VBL windows, so async page-flip commits can be scheduled reliably.
- Future revisions may also use the same daisy-chain checkpoints to support asynchronous garbage collection slices and asynchronous Mockingboard service/update handling.
- Phase 1 audio uses a single-voice Apple speaker mixer stub and preserves a Mockingboard-oriented service boundary for later upgrades.
- If no Mockingboard/audio card backend is present, runtime still keeps an accurate kernel-owned master clock and drives speaker fallback against that same timing model.

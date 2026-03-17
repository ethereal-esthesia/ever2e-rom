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

## Suggested Milestones
1. Bring-up: interpreter + minimum boot subset
2. Language core: functions, locals, arithmetic, branching
3. Heap/object model + strings
4. Module paging + service rebinding
5. Native promotion of hot ops (architecture-local JIT path)

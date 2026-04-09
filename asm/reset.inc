; Reset routine.
;
.include "bank_switch.inc"

; reset
; Args:      none
; Clobbers:  A, X, Y, flags
; Returns:   does not return (falls into main_loop)

reset:
    jsr ensure_g65sc02_or_beep_loop
    jsr softswitch_reset_optimal
    ; Bring up the visible startup screen first so the banner appears before the
    ; broader writable-RAM clear begins.
    jsr main_show_startup_banner
    ; Fill all writable main RAM except text pages with NUL.
    lda #$00
    jsr clear_ram_unrolled_fast
    jsr rng_seed_from_vbl
    ; Startup chirp.
    jsr babeep

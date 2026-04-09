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
    lda #DISPLAY_ALTCHARSET
    jsr display_set
    lda #INVFLG_RESET_STATE
    sta INVFLG
    ; Fill all writable main RAM except text pages with NUL.
    lda #$00
    jsr clear_ram_unrolled_fast
    ; Clear the currently visible text screen through the shared display helper
    ; so startup uses the same path as the rest of the ROM.
    jsr display_text_clear_visible
    jsr display_text_home
    ; Startup chirp.
    jsr babeep

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
    lda #BANK_SWITCH_EXT_RESET_STATE
    jsr bank_switch_apply_extended_state
    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state
    lda #DISPLAY_RESET_STATE
    jsr display_apply_state
    lda #INVFLG_RESET_STATE
    sta INVFLG
    jsr display_text_home
    ; Fill text page 1 + page 2 with spaces.
    bit INVFLG
    bpl @inverse_spaces
    lda #' ' | $80
    bra @fill_text_pages
@inverse_spaces:
    lda #' '
@fill_text_pages:
    jsr display_text_fill_pages
    ; Fill all writable main RAM except text pages with NUL.
    lda #$00
    jsr clear_ram_unrolled_fast
    ; Startup chirp.
    jsr babeep

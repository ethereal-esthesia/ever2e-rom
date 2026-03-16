; Reset routine.
;
; reset
; Args:      none
; Clobbers:  A, X, Y, flags
; Returns:   does not return (falls into main_loop)

reset:
    jsr ensure_g65sc02_or_beep_loop
    jsr softswitch_reset_optimal
    ; Fill text page 1 + page 2 with spaces.
    lda #$20
    jsr fill_text_pages
    ; Fill all writable main RAM except text pages with NUL.
    lda #$00
    jsr clear_ram_unrolled_fast
    ; Startup chirp.
    jsr babeep

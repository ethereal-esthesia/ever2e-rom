; ever2e-rom main entry
; Assembled with ca65 for 65C02/G65SC02-compatible opcodes.

.setcpu "65C02"

.import softswitch_reset_optimal
.import clear_ram_unrolled_fast
.import fill_text_pages
.import babeep
.import ensure_g65sc02_or_beep_loop

.segment "CODE"

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
    ; Quick audible startup test chirp.
    jsr babeep

main_loop:
    jmp main_loop

; nmi
; Args:      none
; Clobbers:  none
; Returns:   RTI
nmi:
    rti

; irq
; Args:      none
; Clobbers:  none
; Returns:   RTI
irq:
    rti

.segment "VECTORS"
    .word nmi
    .word reset
    .word irq

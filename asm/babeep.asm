; Cute "ba-beep" routine using the Apple II speaker soft-switch at $C030.
; Produces a short two-part chirp.

.setcpu "65C02"

.export babeep
.include "tone.inc"

.segment "CODE"

; babeep
; Args:      none
; Clobbers:  A, Y, flags
; Returns:   RTS
babeep:
    ; "ba": lower, slightly slower chirp.
    lda #$96            ; duration outer (50x vs #$03 baseline)
    jsr tone_play

    ; brief pause between syllables
    ldy #$A0
@gap:
    dey
    bne @gap

    ; "beep": brighter/faster chirp.
    lda #$96            ; duration outer (50x vs #$03 baseline)
    jsr tone_play

    rts

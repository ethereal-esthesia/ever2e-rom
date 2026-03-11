; Cute "ba-beep" routine using the Apple II speaker soft-switch at $C030.
; Produces a short two-part chirp.

.setcpu "65C02"

.export babeep
.include "tone.inc"

.segment "CODE"

; babeep
; Args:      none
; Clobbers:  A, X, Y, flags
; Returns:   RTS
babeep:
    ; "ba": lower, slightly slower chirp.
    ; tone_play now toggles twice per inner step, so delay is doubled
    ; versus the previous single-toggle tuning.
    lda #$01            ; duration outer
    ldx #$68            ; frequency delay
    jsr tone_play

    ; brief pause between syllables
    ldy #$A0
@gap:
    dey
    bne @gap

    ; "beep": brighter/faster chirp.
    ; delay doubled to preserve rough pitch after double-toggle change.
    lda #$01            ; duration outer
    ldx #$30            ; frequency delay
    jsr tone_play

    rts

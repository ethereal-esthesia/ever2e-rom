; Reusable speaker tone routine for Apple II at $C030.
;
; tone_play
; Args:
;   A = duration outer count (0 => no sound)
; Clobbers:
;   A, Y, flags
; Returns:
;   RTS
; Notes:
; - Duration is two nested loops:
;   - Inner loop always runs 16 iterations.
;   - Outer loop count is provided by caller in A.
; - Uses stack to hold remaining outer count (no RAM/BSS required).

.setcpu "65C02"

.export tone_play

.segment "CODE"

tone_play:
    cmp #$00
    beq @done
    pha                 ; save remaining outer count

@outer:
    ldy #$10
@inner:
    bit $C030           ; speaker toggle #1
    dey
    bne @inner

    pla                 ; remaining outer count
    dea
    beq @done
    pha
    jmp @outer

@done:
    rts

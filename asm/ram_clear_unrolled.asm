; Clear immediately writable main RAM as fast as practical from ROM code.
; Strategy:
; - Use X from 0..255 once.
; - Unroll page stores across selected page ranges in the loop body.
; - Clear byte is provided in A by caller (loaded once before loop).
; Notes:
; - $0000-$01FF (ZP + stack) is intentionally left untouched so JSR/RTS
;   remain safe during this routine.
; - $0400-$0BFF (text pages 1/2) is intentionally skipped here.

.setcpu "65C02"

.export clear_ram_unrolled_fast

.segment "CODE"

; clear_ram_unrolled_fast
; Args:      A = fill byte
; Clobbers:  X, flags (A preserved)
; Returns:   RTS
clear_ram_unrolled_fast:
    ldx #$00

@xloop:
    ; $0200-$03FF
    .repeat $02, I
        sta $0200 + ($100 * I), x
    .endrepeat

    ; $0C00-$BFFF (skip $0400-$0BFF text pages)
    .repeat $B4, I
        sta $0C00 + ($100 * I), x
    .endrepeat

    inx
    beq @done
    jmp @xloop
@done:
    rts

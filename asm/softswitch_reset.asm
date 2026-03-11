; Reset key IIe soft-switches into a deterministic baseline state.
; This is tuned for text/page1/main-RAM startup behavior before ROM work.

.setcpu "65C02"

.export softswitch_reset_optimal

.segment "CODE"

; softswitch_reset_optimal
; Args:      none
; Clobbers:  A, flags
; Returns:   RTS
softswitch_reset_optimal:
    ; Clear keyboard strobe.
    bit $C010

    ; Ensure 80STORE is cleared (emulator model uses C000 write side effect).
    lda #$00
    sta $C000

    ; Main/aux routing: reads+writes from main RAM, main ZP/stack.
    bit $C002      ; RAMRD off
    bit $C004      ; RAMWRT off
    bit $C008      ; ALTZP off

    ; ROM/slot routing.
    bit $C007      ; INTCXROM on (internal ROM in CNXX)
    bit $C00B      ; SLOTC3ROM on
    bit $CFFF      ; clear INTC8ROM latch

    ; Text/video baseline.
    bit $C00C      ; 80COL off
    bit $C00E      ; ALTCHARSET off
    bit $C051      ; TEXT on
    bit $C052      ; MIXED off
    bit $C054      ; PAGE2 off (page 1)
    bit $C056      ; HIRES off

    ; Clear annunciators.
    bit $C058
    bit $C05A
    bit $C05C
    bit $C05E

    ; Language-card baseline: ROM visible, writes disabled.
    bit $C082

    rts

; Reset key IIe soft-switches into a deterministic baseline state.
; This is tuned for text/page1/main-RAM startup behavior before ROM work.

.setcpu "65C02"

.import bank_switch_common_reset
.import bank_switch_apply_extended_state
.import display_apply_state

.export softswitch_reset_optimal

SOFTSWITCH_RESET_EXT_STATE = $41   ; INTCXROM | SLOTC3ROM
SOFTSWITCH_RESET_DISPLAY_STATE = $08 ; TEXT

.segment "CODE"

; softswitch_reset_optimal
; Args:      none
; Clobbers:  A, flags
; Returns:   RTS
softswitch_reset_optimal:
    ; Clear keyboard strobe.
    bit $C010

    ; Reset tracked bank/display state through the shared helper layer.
    lda #$FF
    jsr bank_switch_common_reset
    lda #SOFTSWITCH_RESET_EXT_STATE
    jsr bank_switch_apply_extended_state
    lda #SOFTSWITCH_RESET_DISPLAY_STATE
    jsr display_apply_state

    rts

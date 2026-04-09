; Display soft-switch helper for the IIe subset we actively drive.
;
; Input:
;   A = new display state byte
;
; Persistent state:
;   `DISPLAY_STATE` at $FE holds the last state applied by this helper.
;   The byte lives in main ZP as defined by `bank_switch.inc`; this routine
;   temporarily forces ALTZP off while updating it so the tracked state stays in
;   main memory even if the caller arrived with auxiliary ZP/stack active.
;
; Bit layout in DISPLAY_STATE:
;   bit 0: 80STORE
;   bit 1: PAGE2
;   bit 2: HIRES
;   bit 3: TEXT
;   bit 4: MIXED
;   bit 5: 80COL
;   bit 6: ALTCHARSET
;   bit 7: AN3 / double-hires gate
;
; Notes:
; - Set bits mean "switch on"; clear bits mean "switch off".
; - `DISPLAY_AN3` is an abstract "DHGR gate" bit. In the current emulator/runtime,
;   enabling that mode maps to a write to $C05E (AN3 clear), matching the prior
;   known-good DHGR setup sequence.
; - This helper reapplies the full subset each time rather than diffing.

.setcpu "65C02"

.include "bank_switch.inc"

.export display_apply_state

DISPLAY_80STORE   = $01
DISPLAY_PAGE2     = $02
DISPLAY_HIRES     = $04
DISPLAY_TEXT      = $08
DISPLAY_MIXED     = $10
DISPLAY_80COL     = $20
DISPLAY_ALTCHARSET = $40
DISPLAY_AN3        = $80

DISPLAY_RESET_STATE = DISPLAY_TEXT

.segment "CODE"

; display_apply_state
; Args:      A = new display state byte
; Clobbers:  A, X, Y, flags, $FE in main ZP
; Returns:   RTS
display_apply_state:
    tax
    lda $C016
    tay

    txa
    sta $C008
    stx DISPLAY_STATE

    txa
    and #DISPLAY_80STORE
    beq @store_off
    sta $C001
    jmp @page_check
@store_off:
    sta $C000

@page_check:
    txa
    and #DISPLAY_PAGE2
    beq @page1
    sta $C055
    jmp @hires_check
@page1:
    sta $C054

@hires_check:
    txa
    and #DISPLAY_HIRES
    beq @hires_off
    sta $C057
    jmp @text_check
@hires_off:
    sta $C056

@text_check:
    txa
    and #DISPLAY_TEXT
    beq @graphics_on
    sta $C051
    jmp @mixed_check
@graphics_on:
    sta $C050

@mixed_check:
    txa
    and #DISPLAY_MIXED
    beq @mixed_off
    sta $C053
    jmp @col80_check
@mixed_off:
    sta $C052

@col80_check:
    txa
    and #DISPLAY_80COL
    beq @col80_off
    sta $C00D
    jmp @altcharset_check
@col80_off:
    sta $C00C

@altcharset_check:
    txa
    and #DISPLAY_ALTCHARSET
    beq @altcharset_off
    sta $C00F
    jmp @an3_check
@altcharset_off:
    sta $C00E

@an3_check:
    txa
    and #DISPLAY_AN3
    beq @an3_off
    sta $C05E
    jmp @restore_altzp
@an3_off:
    sta $C05F
@restore_altzp:
    tya
    bpl @done
    lda #$00
    sta $C009
@done:
    rts

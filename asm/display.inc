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
.export display_set
.export display_reset
.export display_text_write_char_clipped

DISPLAY_80STORE   = $01
DISPLAY_PAGE2     = $02
DISPLAY_HIRES     = $04
DISPLAY_TEXT      = $08
DISPLAY_MIXED     = $10
DISPLAY_80COL     = $20
DISPLAY_ALTCHARSET = $40
DISPLAY_AN3        = $80

DISPLAY_RESET_STATE = DISPLAY_TEXT

DISPLAY_TEXT_PTR_LO = ZP_SCRATCH_8
DISPLAY_TEXT_PTR_HI = ZP_SCRATCH_9
DISPLAY_TEXT_CHAR   = ZP_SCRATCH_A
DISPLAY_TEXT_COL    = ZP_SCRATCH_B
DISPLAY_TEXT_SAVE   = ZP_SCRATCH_C

.segment "CODE"

; display_apply_state
; Args:      A = new display state byte
; Clobbers:  A, X, Y, flags, $FE in main ZP
; Returns:   RTS
display_set:
    tax
    lda $C016
    tay

    txa
    sta $C008
    txa
    ora DISPLAY_STATE
    tax

    tya
    bpl :+
    lda #$00
    sta $C009
:
    txa
    jmp display_apply_state

display_reset:
    tax
    lda $C016
    tay

    txa
    sta $C008
    txa
    eor #$FF
    and DISPLAY_STATE
    tax

    tya
    bpl :+
    lda #$00
    sta $C009
:
    txa
    jmp display_apply_state

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

; display_text_write_char_clipped
; Args:      A = character, X = clipped column, Y = clipped row
; Clobbers:  A, X, Y, flags, ZP_SCRATCH_8-ZP_SCRATCH_C
; Returns:   RTS
; Notes:
; - Detects 80-column mode from the IIe 80COL status read at $C01F.
; - In 80-column mode, even columns map to auxiliary text memory and odd columns
;   map to main text memory, matching the JVM/py display model.
; - Coordinates are expected to be pre-clipped by the caller.
display_text_write_char_clipped:
    sta DISPLAY_TEXT_CHAR
    stx DISPLAY_TEXT_COL

    lda display_text_row_base_lo,y
    sta DISPLAY_TEXT_PTR_LO
    lda display_text_row_base_hi,y
    sta DISPLAY_TEXT_PTR_HI

    bit $C018
    bmi @base_ready
    bit $C01C
    bpl @base_ready
    clc
    lda DISPLAY_TEXT_PTR_HI
    adc #$04
    sta DISPLAY_TEXT_PTR_HI

@base_ready:
    bit $C01F
    bmi @text80

    ldx DISPLAY_TEXT_COL
    txa
    clc
    adc DISPLAY_TEXT_PTR_LO
    sta DISPLAY_TEXT_PTR_LO
    bcc :+
    inc DISPLAY_TEXT_PTR_HI
:
    bit $C018
    bmi @write40_now
    lda $C014
    sta DISPLAY_TEXT_SAVE
    bit $C004

@write40_now:
    ldy #$00
    lda DISPLAY_TEXT_CHAR
    ora #$80
    sta (DISPLAY_TEXT_PTR_LO),y

    bit $C018
    bmi @text_write_done
    lda DISPLAY_TEXT_SAVE
    bpl @text_write_done
    bit $C005
    rts

@text_write_done:
    rts

@text80:
    ldx DISPLAY_TEXT_COL
    txa
    lsr a
    clc
    adc DISPLAY_TEXT_PTR_LO
    sta DISPLAY_TEXT_PTR_LO
    bcc :+
    inc DISPLAY_TEXT_PTR_HI
:
    lda DISPLAY_TEXT_CHAR
    ora #$80
    sta DISPLAY_TEXT_CHAR

    bit $C018
    bmi @text80_page2_route

    lda $C014
    sta DISPLAY_TEXT_SAVE
    lda DISPLAY_TEXT_COL
    and #$01
    bne @text80_main_ramwrt
    bit $C005
    jmp @text80_store_ramwrt

@text80_main_ramwrt:
    bit $C004

@text80_store_ramwrt:
    ldy #$00
    lda DISPLAY_TEXT_CHAR
    sta (DISPLAY_TEXT_PTR_LO),y
    lda DISPLAY_TEXT_SAVE
    bmi @restore_ramwrt_on
    bit $C004
    rts

@restore_ramwrt_on:
    bit $C005
    rts

@text80_page2_route:
    lda $C01C
    sta DISPLAY_TEXT_SAVE
    lda DISPLAY_TEXT_COL
    and #$01
    bne @text80_main_page2
    bit $C055
    jmp @text80_store_page2

@text80_main_page2:
    bit $C054

@text80_store_page2:
    ldy #$00
    lda DISPLAY_TEXT_CHAR
    sta (DISPLAY_TEXT_PTR_LO),y
    lda DISPLAY_TEXT_SAVE
    bmi @restore_page2_on
    bit $C054
    rts

@restore_page2_on:
    bit $C055
    rts

display_text_row_base_lo:
    .byte <$0400, <$0480, <$0500, <$0580, <$0600, <$0680, <$0700, <$0780
    .byte <$0428, <$04A8, <$0528, <$05A8, <$0628, <$06A8, <$0728, <$07A8
    .byte <$0450, <$04D0, <$0550, <$05D0, <$0650, <$06D0, <$0750, <$07D0

display_text_row_base_hi:
    .byte >$0400, >$0480, >$0500, >$0580, >$0600, >$0680, >$0700, >$0780
    .byte >$0428, >$04A8, >$0528, >$05A8, >$0628, >$06A8, >$0728, >$07A8
    .byte >$0450, >$04D0, >$0550, >$05D0, >$0650, >$06D0, >$0750, >$07D0

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
.export display_text_clear_visible
.export display_text_home
.export display_text_write_null_terminated_string_ax_y_padded
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

DISPLAY_TEXT_POINTER_LO = ZP_SCRATCH_8
DISPLAY_TEXT_POINTER_HI = ZP_SCRATCH_9
DISPLAY_TEXT_SOURCE_CHARACTER   = ZP_SCRATCH_A
DISPLAY_TEXT_TARGET_COLUMN    = ZP_SCRATCH_B
DISPLAY_TEXT_SAVED_ROUTE_STATE   = ZP_SCRATCH_C
DISPLAY_TEXT_VISIBLE_WIDTH  = ZP_SCRATCH_D
DISPLAY_TEXT_TARGET_ROW    = ZP_SCRATCH_E
DISPLAY_TEXT_ATTRIBUTE_MASK   = ZP_SCRATCH_F

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

; display_text_clear_visible
; Args:      none
; Clobbers:  A, X, Y, flags, ZP_SCRATCH_8-ZP_SCRATCH_F
; Returns:   RTS
; Notes:
; - Clears only the currently visible text page.
; - Fills with normal or inverse spaces based on INVFLG bit 7, matching the
;   80-column firmware's normal-vs-inverse choice.
; - Clears line-by-line so 80-column mode can fill the main/aux halves for each
;   visible row directly instead of routing every single character write.
display_text_clear_visible:
    jsr display_text_space_from_invflg
    sta DISPLAY_TEXT_SOURCE_CHARACTER
    stz DISPLAY_TEXT_TARGET_ROW

    bit $C01F
    bmi @clear_80col

@clear_40col_row:
    ldy DISPLAY_TEXT_TARGET_ROW
    jsr display_text_prepare_row_base_for_visible_page
    bit $C018
    bmi @clear_40col_store
    lda $C014
    sta DISPLAY_TEXT_SAVED_ROUTE_STATE
    lda #BANK_SWITCH_COMMON_RAMWRT
    jsr bank_switch_common_reset

@clear_40col_store:
    jsr display_text_fill_40_bytes_from_pointer
    bit $C018
    bmi @clear_40col_next
    lda DISPLAY_TEXT_SAVED_ROUTE_STATE
    bpl @clear_40col_next
    lda #BANK_SWITCH_COMMON_RAMWRT
    jsr bank_switch_common_set

@clear_40col_next:
    inc DISPLAY_TEXT_TARGET_ROW
    lda DISPLAY_TEXT_TARGET_ROW
    cmp #24
    bne @clear_40col_row
    rts

@clear_80col:
    bit $C018
    bmi @clear_80col_page_route

@clear_80col_ram_route_row:
    ldy DISPLAY_TEXT_TARGET_ROW
    jsr display_text_prepare_row_base_for_visible_page
    lda $C014
    sta DISPLAY_TEXT_SAVED_ROUTE_STATE
    lda #BANK_SWITCH_COMMON_RAMWRT
    jsr bank_switch_common_reset
    jsr display_text_fill_40_bytes_from_pointer
    lda #BANK_SWITCH_COMMON_RAMWRT
    jsr bank_switch_common_set
    jsr display_text_fill_40_bytes_from_pointer
    lda DISPLAY_TEXT_SAVED_ROUTE_STATE
    bmi @clear_80col_restore_ramwrt_on
    lda #BANK_SWITCH_COMMON_RAMWRT
    jsr bank_switch_common_reset
    bra @clear_80col_next

@clear_80col_restore_ramwrt_on:
    lda #BANK_SWITCH_COMMON_RAMWRT
    jsr bank_switch_common_set
    bra @clear_80col_next

@clear_80col_page_route:
    ldy DISPLAY_TEXT_TARGET_ROW
    jsr display_text_prepare_row_base_for_visible_page
    lda $C01C
    sta DISPLAY_TEXT_SAVED_ROUTE_STATE
    bit $C054
    jsr display_text_fill_40_bytes_from_pointer
    bit $C055
    jsr display_text_fill_40_bytes_from_pointer
    lda DISPLAY_TEXT_SAVED_ROUTE_STATE
    bmi @clear_80col_restore_page2_on
    bit $C054
    bra @clear_80col_next

@clear_80col_restore_page2_on:
    bit $C055

@clear_80col_next:
    inc DISPLAY_TEXT_TARGET_ROW
    lda DISPLAY_TEXT_TARGET_ROW
    cmp #24
    bne @clear_80col
    rts

; display_text_home
; Args:      none
; Clobbers:  A, Y, flags
; Returns:   RTS
; Notes:
; - Resets the stock text cursor state (`CH`/`CV`) in canonical main ZP.
display_text_home:
    lda $C016
    tay
    lda #$00
    sta $C008
    stz CH
    stz CV
    tya
    bpl @home_done
    lda #$00
    sta $C009
@home_done:
    rts

; display_text_write_null_terminated_string_ax_y_padded
; Args:      A = string pointer low, X = string pointer high, Y = exact number
;            of spaces to emit after the terminating NUL
; Clobbers:  A, X, Y, flags, ZP_SCRATCH_8-ZP_SCRATCH_F
; Returns:   RTS
; Notes:
; - Writes through the shared `CH`/`CV` cursor state and advances `CH`.
; - Padding spaces use the current `INVFLG` encoding, just like normal text output.
; - Calls `display_text_write_char_clipped` once per visible character so string
;   output stays aligned with the single-character path.
display_text_write_null_terminated_string_ax_y_padded:
    sta DISPLAY_TEXT_POINTER_LO
    stx DISPLAY_TEXT_POINTER_HI
    sty DISPLAY_TEXT_VISIBLE_WIDTH

@string_loop:
    ldy #$00
    lda (DISPLAY_TEXT_POINTER_LO),y
    beq @padding_loop
    sta DISPLAY_TEXT_SOURCE_CHARACTER

    lda DISPLAY_TEXT_POINTER_LO
    pha
    lda DISPLAY_TEXT_POINTER_HI
    pha

    lda DISPLAY_TEXT_SOURCE_CHARACTER
    ldx CH
    ldy CV
    jsr display_text_write_char_clipped
    inc CH

    pla
    sta DISPLAY_TEXT_POINTER_HI
    pla
    sta DISPLAY_TEXT_POINTER_LO

    inc DISPLAY_TEXT_POINTER_LO
    bne @string_loop
    inc DISPLAY_TEXT_POINTER_HI
    bra @string_loop

@padding_loop:
    lda DISPLAY_TEXT_VISIBLE_WIDTH
    beq @string_done
    dec DISPLAY_TEXT_VISIBLE_WIDTH
    lda #' '
    ldx CH
    ldy CV
    jsr display_text_write_char_clipped
    inc CH
    bra @padding_loop

@string_done:
    rts

; display_text_write_char_clipped
; Args:      A = character, X = clipped column, Y = clipped row
; Clobbers:  A, X, Y, flags, ZP_SCRATCH_8-ZP_SCRATCH_F
; Returns:   RTS
; Notes:
; - Detects 80-column mode from the IIe 80COL status read at $C01F.
; - In 80-column mode, even columns map to auxiliary text memory and odd columns
;   map to main text memory, matching the JVM/py display model.
; - Standard 40-column text uses the stock `($80 | ASCII) & INVFLG` encoding.
; - In standard 40-column text, lowercase ASCII is folded to uppercase before
;   encoding because the primary character set does not provide distinct
;   lowercase glyphs.
; - Alternate/80-column text uses only bit 7 of `INVFLG`; flash falls back to
;   inverse there because the firmware path does not support flashing text.
; - When alternate characters are active, inverse `@`-`_` is remapped from
;   `$40-$5F` down to `$00-$1F` so generic ASCII output does not accidentally
;   select MouseText glyph codes. This keeps direct stores aligned with Apple's
;   recommendation for code that writes text memory instead of going through the
;   firmware's MouseText-aware output path.
; - Coordinates are expected to be pre-clipped by the caller.
display_text_write_char_clipped:
    sta DISPLAY_TEXT_SOURCE_CHARACTER
    stx DISPLAY_TEXT_TARGET_COLUMN
    sty DISPLAY_TEXT_TARGET_ROW

    jsr display_text_load_invflg_main
    sta DISPLAY_TEXT_ATTRIBUTE_MASK

    bit $C01F
    bmi @encode_alternate_text

    jsr display_text_load_display_state_main
    and #DISPLAY_ALTCHARSET
    bne @encode_alternate_text

    lda DISPLAY_TEXT_SOURCE_CHARACTER
    cmp #'a'
    bcc @encode_primary_text
    cmp #'z' + 1
    bcs @encode_primary_text
    and #$DF
    bra @encode_primary_apply

@encode_primary_text:
    lda DISPLAY_TEXT_SOURCE_CHARACTER

@encode_primary_apply:
    ora #$80
    and DISPLAY_TEXT_ATTRIBUTE_MASK
    jmp display_text_write_char_prepared

@encode_alternate_text:
    lda DISPLAY_TEXT_ATTRIBUTE_MASK
    and #$80
    bne @encode_alternate_normal

    lda DISPLAY_TEXT_SOURCE_CHARACTER
    cmp #'@'
    bcc @encode_alternate_inverse_direct
    cmp #$60
    bcs @encode_alternate_inverse_direct
    sec
    sbc #$40
    jmp display_text_write_char_prepared

@encode_alternate_inverse_direct:
    lda DISPLAY_TEXT_SOURCE_CHARACTER
    and #$7F
    jmp display_text_write_char_prepared

@encode_alternate_normal:
    lda DISPLAY_TEXT_SOURCE_CHARACTER
    ora #$80

display_text_write_char_prepared:
    sta DISPLAY_TEXT_SOURCE_CHARACTER
    jmp display_text_write_char_saved

display_text_write_encoded_char_clipped:
    sta DISPLAY_TEXT_SOURCE_CHARACTER
    stx DISPLAY_TEXT_TARGET_COLUMN
    sty DISPLAY_TEXT_TARGET_ROW

display_text_write_char_saved:
    ldy DISPLAY_TEXT_TARGET_ROW
    jsr display_text_prepare_row_base_for_visible_page
    bit $C01F
    bmi @text80

    ldx DISPLAY_TEXT_TARGET_COLUMN
    txa
    clc
    adc DISPLAY_TEXT_POINTER_LO
    sta DISPLAY_TEXT_POINTER_LO
    bcc :+
    inc DISPLAY_TEXT_POINTER_HI
:
    bit $C018
    bmi @write40_now
    lda $C014
    sta DISPLAY_TEXT_SAVED_ROUTE_STATE
    lda #BANK_SWITCH_COMMON_RAMWRT
    jsr bank_switch_common_reset

@write40_now:
    ldy #$00
    lda DISPLAY_TEXT_SOURCE_CHARACTER
    sta (DISPLAY_TEXT_POINTER_LO),y

    bit $C018
    bmi @text_write_done
    lda DISPLAY_TEXT_SAVED_ROUTE_STATE
    bpl @text_write_done
    lda #BANK_SWITCH_COMMON_RAMWRT
    jsr bank_switch_common_set
    rts

@text_write_done:
    rts

@text80:
    ldx DISPLAY_TEXT_TARGET_COLUMN
    txa
    lsr a
    clc
    adc DISPLAY_TEXT_POINTER_LO
    sta DISPLAY_TEXT_POINTER_LO
    bcc :+
    inc DISPLAY_TEXT_POINTER_HI
:
    bit $C018
    bmi @text80_page2_route

    lda $C014
    sta DISPLAY_TEXT_SAVED_ROUTE_STATE
    lda DISPLAY_TEXT_TARGET_COLUMN
    and #$01
    bne @text80_main_ramwrt
    lda #BANK_SWITCH_COMMON_RAMWRT
    jsr bank_switch_common_set
    jmp @text80_store_ramwrt

@text80_main_ramwrt:
    lda #BANK_SWITCH_COMMON_RAMWRT
    jsr bank_switch_common_reset

@text80_store_ramwrt:
    ldy #$00
    lda DISPLAY_TEXT_SOURCE_CHARACTER
    sta (DISPLAY_TEXT_POINTER_LO),y
    lda DISPLAY_TEXT_SAVED_ROUTE_STATE
    bmi @restore_ramwrt_on
    lda #BANK_SWITCH_COMMON_RAMWRT
    jsr bank_switch_common_reset
    rts

@restore_ramwrt_on:
    lda #BANK_SWITCH_COMMON_RAMWRT
    jsr bank_switch_common_set
    rts

@text80_page2_route:
    lda $C01C
    sta DISPLAY_TEXT_SAVED_ROUTE_STATE
    lda DISPLAY_TEXT_TARGET_COLUMN
    and #$01
    bne @text80_main_page2
    bit $C055
    jmp @text80_store_page2

@text80_main_page2:
    bit $C054

@text80_store_page2:
    ldy #$00
    lda DISPLAY_TEXT_SOURCE_CHARACTER
    sta (DISPLAY_TEXT_POINTER_LO),y
    lda DISPLAY_TEXT_SAVED_ROUTE_STATE
    bmi @restore_page2_on
    bit $C054
    rts

@restore_page2_on:
    bit $C055
    rts

display_text_prepare_row_base_for_visible_page:
    lda DISPLAY_TEXT_ROW_BASE_ADDRESS_LO,y
    sta DISPLAY_TEXT_POINTER_LO
    lda DISPLAY_TEXT_ROW_BASE_ADDRESS_HI,y
    sta DISPLAY_TEXT_POINTER_HI

    bit $C018
    bmi @prepared_row_base
    bit $C01C
    bpl @prepared_row_base
    clc
    lda DISPLAY_TEXT_POINTER_HI
    adc #$04
    sta DISPLAY_TEXT_POINTER_HI

@prepared_row_base:
    rts

display_text_fill_40_bytes_from_pointer:
    ldy #$00
    ldx #40
    lda DISPLAY_TEXT_SOURCE_CHARACTER
@fill_40_loop:
    sta (DISPLAY_TEXT_POINTER_LO),y
    iny
    dex
    bne @fill_40_loop
    rts

display_text_load_invflg_main:
    lda $C016
    tay
    lda #$00
    sta $C008
    lda INVFLG
    tax
    tya
    bpl :+
    lda #$00
    sta $C009
:
    txa
    rts

display_text_load_display_state_main:
    lda $C016
    tay
    lda #$00
    sta $C008
    lda DISPLAY_STATE
    tax
    tya
    bpl :+
    lda #$00
    sta $C009
:
    txa
    rts

display_text_space_from_invflg:
    jsr display_text_load_invflg_main
    bmi @normal_space
    lda #' '
    rts
@normal_space:
    lda #' ' | $80
    rts

display_text_current_width:
    bit $C01F
    bmi @width_80
    lda #40
    rts
@width_80:
    lda #80
    rts

DISPLAY_TEXT_ROW_BASE_ADDRESS_LO:
    .byte <$0400, <$0480, <$0500, <$0580, <$0600, <$0680, <$0700, <$0780
    .byte <$0428, <$04A8, <$0528, <$05A8, <$0628, <$06A8, <$0728, <$07A8
    .byte <$0450, <$04D0, <$0550, <$05D0, <$0650, <$06D0, <$0750, <$07D0

DISPLAY_TEXT_ROW_BASE_ADDRESS_HI:
    .byte >$0400, >$0480, >$0500, >$0580, >$0600, >$0680, >$0700, >$0780
    .byte >$0428, >$04A8, >$0528, >$05A8, >$0628, >$06A8, >$0728, >$07A8
    .byte >$0450, >$04D0, >$0550, >$05D0, >$0650, >$06D0, >$0750, >$07D0

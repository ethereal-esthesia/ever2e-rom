.ifndef EVER2E_BANK_SWITCH_INC
EVER2E_BANK_SWITCH_INC = 1

; Shared bank/display helpers and the canonical zero-page map for this repo.
;
; `$00-$0F` is a shared scratch window. Routines should alias the bytes they
; need locally, reuse them while active, and not rely on those values surviving
; calls into other routines.
;
; The persistent control bytes at `$FC-$FE` are special: the helpers below
; always force ALTZP to main memory before reading or writing them, so display
; and bank state stays coherent in main ZP even when the caller is currently
; using aux ZP/stack. `bank_switch_apply_common_state` also migrates the
; caller's return address between the main and auxiliary stacks when it changes
; ALTZP, so it can safely return with the newly requested bank state live.

.setcpu "65C02"

.export bank_switch_apply_common_state
.export bank_switch_apply_extended_state
.export bank_switch_common_set
.export bank_switch_common_reset
.export bank_switch_extended_set
.export bank_switch_extended_reset

; Shared ZP scratch area. Alias these locally inside each routine as needed.
ZP_SCRATCH_0 = $00
ZP_SCRATCH_1 = $01
ZP_SCRATCH_2 = $02
ZP_SCRATCH_3 = $03
ZP_SCRATCH_4 = $04
ZP_SCRATCH_5 = $05
ZP_SCRATCH_6 = $06
ZP_SCRATCH_7 = $07
ZP_SCRATCH_8 = $08
ZP_SCRATCH_9 = $09
ZP_SCRATCH_A = $0A
ZP_SCRATCH_B = $0B
ZP_SCRATCH_C = $0C
ZP_SCRATCH_D = $0D
ZP_SCRATCH_E = $0E
ZP_SCRATCH_F = $0F

BANK_SWITCH_EXT_STATE    = $FC
BANK_SWITCH_COMMON_STATE = $FD
DISPLAY_STATE            = $FE

; $FD: common bank switches a program is most likely to use.
BANK_SWITCH_COMMON_LC_READ_RAM = $01
BANK_SWITCH_COMMON_LC_BANK1    = $02
BANK_SWITCH_COMMON_LC_WRITE    = $04
BANK_SWITCH_COMMON_RAMRD       = $08
BANK_SWITCH_COMMON_RAMWRT      = $10
BANK_SWITCH_COMMON_ALTZP       = $20

; $FC: less common routing/latch switches plus AN0-AN2.
BANK_SWITCH_EXT_INTCXROM   = $01
BANK_SWITCH_EXT_INTC8ROM   = $02
BANK_SWITCH_EXT_LC_PREWRITE = $04
BANK_SWITCH_EXT_AN0        = $08
BANK_SWITCH_EXT_AN1        = $10
BANK_SWITCH_EXT_AN2        = $20

BANK_SWITCH_COMMON_RESET_STATE = $00
BANK_SWITCH_EXT_RESET_STATE    = BANK_SWITCH_EXT_INTCXROM

.segment "CODE"

; bank_switch_apply_common_state
; Args:      A = new common bank state byte for $FD
; Clobbers:  A, X, Y, flags, $FC/$FD in main ZP
; Returns:   RTS with the requested ALTZP state live
; Notes:     Use `bank_switch_common_set` / `bank_switch_common_reset` when a
;            caller only wants to update specific bits in the tracked state.
bank_switch_common_set:
    tax
    lda $C016
    tay

    txa
    sta $C008
    txa
    ora BANK_SWITCH_COMMON_STATE
    tax

    tya
    bpl :+
    lda #$00
    sta $C009
:
    txa
    jmp bank_switch_apply_common_state

bank_switch_common_reset:
    tax
    lda $C016
    tay

    txa
    sta $C008
    txa
    eor #$FF
    and BANK_SWITCH_COMMON_STATE
    tax

    tya
    bpl :+
    lda #$00
    sta $C009
:
    txa
    jmp bank_switch_apply_common_state

bank_switch_apply_common_state:
    tax
    lda $C016
    tay

    txa
    sta $C008
    stx BANK_SWITCH_COMMON_STATE

    txa
    and #BANK_SWITCH_COMMON_RAMRD
    beq @ramrd_off
    sta $C003
    jmp @ramwrt_check
@ramrd_off:
    sta $C002

@ramwrt_check:
    txa
    and #BANK_SWITCH_COMMON_RAMWRT
    beq @ramwrt_off
    sta $C005
    jmp @lc_check
@ramwrt_off:
    sta $C004

@lc_check:
    jsr bank_switch_apply_lc_common_main

    txa
    and #BANK_SWITCH_COMMON_ALTZP
    beq @want_main

    tya
    bmi @return_aux

    ; Caller entered on main stack; move return to aux stack before returning.
    pla
    tax
    pla
    tay
    lda #$00
    sta $C009
    tya
    pha
    txa
    pha
    rts

@return_aux:
    lda #$00
    sta $C009
    rts

@want_main:
    tya
    bpl @return_main

    ; Caller entered on aux stack; move return to main stack before returning.
    lda #$00
    sta $C009
    pla
    tax
    pla
    tay
    lda #$00
    sta $C008
    tya
    pha
    txa
    pha
    rts

@return_main:
    rts

; bank_switch_apply_extended_state
; Args:      A = new extended bank state byte for $FC
; Clobbers:  A, X, Y, flags, $FC in main ZP
; Returns:   RTS with the caller's original ALTZP state restored
; Notes:     Use `bank_switch_extended_set` / `bank_switch_extended_reset` when
;            a caller only wants to update specific bits in the tracked state.
bank_switch_extended_set:
    tax
    lda $C016
    tay

    txa
    sta $C008
    txa
    ora BANK_SWITCH_EXT_STATE
    tax

    tya
    bpl :+
    lda #$00
    sta $C009
:
    txa
    jmp bank_switch_apply_extended_state

bank_switch_extended_reset:
    tax
    lda $C016
    tay

    txa
    sta $C008
    txa
    eor #$FF
    and BANK_SWITCH_EXT_STATE
    tax

    tya
    bpl :+
    lda #$00
    sta $C009
:
    txa
    jmp bank_switch_apply_extended_state

bank_switch_apply_extended_state:
    tax
    lda $C016
    tay

    txa
    sta $C008
    stx BANK_SWITCH_EXT_STATE

    txa
    and #BANK_SWITCH_EXT_INTCXROM
    beq @intcxrom_off
    sta $C007
    jmp @intc8_check
@intcxrom_off:
    sta $C006

@intc8_check:
    txa
    and #BANK_SWITCH_EXT_INTC8ROM
    beq @intc8_off
    sta $C00A
    bit $C300
    sta $C00B
    jmp @prewrite_check
@intc8_off:
    sta $CFFF

@prewrite_check:
    jsr bank_switch_apply_extended_prewrite_main

    txa
    and #BANK_SWITCH_EXT_AN0
    beq @an0_off
    sta $C059
    jmp @an1_check
@an0_off:
    sta $C058

@an1_check:
    txa
    and #BANK_SWITCH_EXT_AN1
    beq @an1_off
    sta $C05B
    jmp @an2_check
@an1_off:
    sta $C05A

@an2_check:
    txa
    and #BANK_SWITCH_EXT_AN2
    beq @an2_off
    sta $C05D
    jmp @restore_altzp
@an2_off:
    sta $C05C

@restore_altzp:
    tya
    bpl @done
    lda #$00
    sta $C009
@done:
    rts

; bank_switch_apply_lc_common_main
; Args:      X = BANK_SWITCH_COMMON_STATE (main ZP already active)
; Clobbers:  A, flags, $FC bit 2 in main ZP
; Returns:   RTS
bank_switch_apply_lc_common_main:
    txa
    and #BANK_SWITCH_COMMON_LC_BANK1
    beq @bank2

@bank1:
    txa
    and #BANK_SWITCH_COMMON_LC_WRITE
    beq @bank1_nowrite
    txa
    and #BANK_SWITCH_COMMON_LC_READ_RAM
    beq @bank1_rom_write
    sta $C08B
    sta $C08B
    jmp bank_switch_mark_prewrite_on_main
@bank1_rom_write:
    sta $C089
    sta $C089
    jmp bank_switch_mark_prewrite_on_main
@bank1_nowrite:
    txa
    and #BANK_SWITCH_COMMON_LC_READ_RAM
    beq @bank1_rom_nowrite
    sta $C088
    jmp bank_switch_mark_prewrite_off_main
@bank1_rom_nowrite:
    sta $C08A
    jmp bank_switch_mark_prewrite_off_main

@bank2:
    txa
    and #BANK_SWITCH_COMMON_LC_WRITE
    beq @bank2_nowrite
    txa
    and #BANK_SWITCH_COMMON_LC_READ_RAM
    beq @bank2_rom_write
    sta $C083
    sta $C083
    jmp bank_switch_mark_prewrite_on_main
@bank2_rom_write:
    sta $C081
    sta $C081
    jmp bank_switch_mark_prewrite_on_main
@bank2_nowrite:
    txa
    and #BANK_SWITCH_COMMON_LC_READ_RAM
    beq @bank2_rom_nowrite
    sta $C080
    jmp bank_switch_mark_prewrite_off_main
@bank2_rom_nowrite:
    sta $C082
    jmp bank_switch_mark_prewrite_off_main

; bank_switch_apply_extended_prewrite_main
; Args:      X = BANK_SWITCH_EXT_STATE (main ZP already active)
; Clobbers:  A, flags, $FC bit 2 in main ZP
; Returns:   RTS
bank_switch_apply_extended_prewrite_main:
    lda BANK_SWITCH_COMMON_STATE
    and #BANK_SWITCH_COMMON_LC_WRITE
    bne @force_prewrite_on

    txa
    and #BANK_SWITCH_EXT_LC_PREWRITE
    beq @clear_prewrite

    lda BANK_SWITCH_COMMON_STATE
    and #(BANK_SWITCH_COMMON_LC_BANK1 | BANK_SWITCH_COMMON_LC_READ_RAM)
    cmp #(BANK_SWITCH_COMMON_LC_BANK1 | BANK_SWITCH_COMMON_LC_READ_RAM)
    beq @bank1_ram_prewrite
    cmp #BANK_SWITCH_COMMON_LC_BANK1
    beq @bank1_rom_prewrite
    cmp #BANK_SWITCH_COMMON_LC_READ_RAM
    beq @bank2_ram_prewrite
    sta $C081
    rts
@bank1_ram_prewrite:
    sta $C08B
    rts
@bank1_rom_prewrite:
    sta $C089
    rts
@bank2_ram_prewrite:
    sta $C083
    rts

@clear_prewrite:
    lda BANK_SWITCH_COMMON_STATE
    and #(BANK_SWITCH_COMMON_LC_BANK1 | BANK_SWITCH_COMMON_LC_READ_RAM)
    cmp #(BANK_SWITCH_COMMON_LC_BANK1 | BANK_SWITCH_COMMON_LC_READ_RAM)
    beq @bank1_ram_clear
    cmp #BANK_SWITCH_COMMON_LC_BANK1
    beq @bank1_rom_clear
    cmp #BANK_SWITCH_COMMON_LC_READ_RAM
    beq @bank2_ram_clear
    sta $C082
    rts
@bank1_ram_clear:
    sta $C088
    rts
@bank1_rom_clear:
    sta $C08A
    rts
@bank2_ram_clear:
    sta $C080
    rts

@force_prewrite_on:
    jmp bank_switch_mark_prewrite_on_main

bank_switch_mark_prewrite_on_main:
    lda BANK_SWITCH_EXT_STATE
    ora #BANK_SWITCH_EXT_LC_PREWRITE
    sta BANK_SWITCH_EXT_STATE
    rts

bank_switch_mark_prewrite_off_main:
    lda BANK_SWITCH_EXT_STATE
    and #($FF ^ BANK_SWITCH_EXT_LC_PREWRITE)
    sta BANK_SWITCH_EXT_STATE
    rts

.endif

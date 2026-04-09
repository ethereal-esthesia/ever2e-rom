.ifndef EVER2E_BANK_SWITCH_INC
EVER2E_BANK_SWITCH_INC = 1

; Shared bank/display helpers and the canonical zero-page map for this repo.
;
; `$00-$0F` is a shared scratch window. Routines should alias the bytes they
; need locally, reuse them while active, and not rely on those values surviving
; calls into other routines.
;
; The repo also reserves a small stock-style text state block in main ZP:
; `CH` ($24), `CV` ($25), and `INVFLG` ($32). Text helpers use these Apple
; conventions for cursor position and text attribute selection.
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

CH                   = $24
CV                   = $25
INVFLG               = $32
INVFLG_NORMAL        = $FF
INVFLG_FLASH         = $7F
INVFLG_INVERSE       = $3F
INVFLG_RESET_STATE   = INVFLG_INVERSE

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
BANK_SWITCH_EXT_SLOTC3ROM  = $40

BANK_SWITCH_COMMON_RESET_STATE = $00
BANK_SWITCH_EXT_RESET_STATE    = BANK_SWITCH_EXT_INTCXROM | BANK_SWITCH_EXT_SLOTC3ROM
BANK_SWITCH_TRAMP             = $0300
BANK_SWITCH_COMMON_FAST_MASK  = BANK_SWITCH_COMMON_RAMRD | BANK_SWITCH_COMMON_RAMWRT

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
    pha
    ora BANK_SWITCH_COMMON_STATE
    tax
    pla
    jsr bank_switch_try_fast_common_update
    bcs @common_set_done

    tya
    bpl :+
    lda #$00
    sta $C009
:
    txa
    jmp bank_switch_apply_common_state

@common_set_done:
    rts

bank_switch_common_reset:
    tax
    lda $C016
    tay

    txa
    sta $C008
    txa
    pha
    eor #$FF
    and BANK_SWITCH_COMMON_STATE
    tax
    pla
    jsr bank_switch_try_fast_common_update
    bcs @common_reset_done

    tya
    bpl :+
    lda #$00
    sta $C009
:
    txa
    jmp bank_switch_apply_common_state

@common_reset_done:
    rts

; bank_switch_try_fast_common_update
; Args:      A = caller mask, X = new common state, Y = original $C016 read
; Clobbers:  A, X, Y, flags
; Returns:   carry set if the update was applied inline and ALTZP restored;
;            carry clear if the caller must fall back to bank_switch_apply_common_state
; Notes:
; - The fast path intentionally only handles RAMRD/RAMWRT changes. That keeps
;   the common "temporary main/aux routing" case simple for callers while still
;   recording the tracked state in `$FD`.
bank_switch_try_fast_common_update:
    and #($FF ^ BANK_SWITCH_COMMON_FAST_MASK)
    bne @common_try_slow

    stx BANK_SWITCH_COMMON_STATE

    txa
    and #BANK_SWITCH_COMMON_RAMRD
    beq @common_try_ramrd_off
    sta $C003
    bra @common_try_ramwrt_check
@common_try_ramrd_off:
    sta $C002

@common_try_ramwrt_check:
    txa
    and #BANK_SWITCH_COMMON_RAMWRT
    beq @common_try_ramwrt_off
    sta $C005
    bra @common_try_restore_altzp
@common_try_ramwrt_off:
    sta $C004

@common_try_restore_altzp:
    tya
    bpl @common_try_fast_done
    lda #$00
    sta $C009
@common_try_fast_done:
    sec
    rts

@common_try_slow:
    clc
    rts

bank_switch_apply_common_state:
    tax
    lda $C016
    tay

    txa
    sta $C008
    stx BANK_SWITCH_COMMON_STATE

    lda #$00
    sta $C002
    sta $C004

    ldx #$00
@copy_common_loop:
    lda bank_switch_common_tramp_start,x
    sta BANK_SWITCH_TRAMP,x
    inx
    cpx #(bank_switch_common_tramp_end - bank_switch_common_tramp_start)
    bne @copy_common_loop

    lda #$00
    sta $C005
    ldx #$00
@copy_common_aux_loop:
    lda bank_switch_common_tramp_start,x
    sta BANK_SWITCH_TRAMP,x
    inx
    cpx #(bank_switch_common_tramp_end - bank_switch_common_tramp_start)
    bne @copy_common_aux_loop
    lda #$00
    sta $C004

    ldx BANK_SWITCH_COMMON_STATE
    jmp BANK_SWITCH_TRAMP

; bank_switch_common_tramp_start
; Args:      X = BANK_SWITCH_COMMON_STATE, Y = original $C016 read
; Clobbers:  A, X, Y, flags, $FC/$FD in main ZP
; Returns:   RTS directly to the original caller
bank_switch_common_tramp_start:
    lda #$00
    sta $C002
    sta $C004

    txa
    and #BANK_SWITCH_COMMON_LC_BANK1
    beq @c_bank2

@c_bank1:
    txa
    and #BANK_SWITCH_COMMON_LC_WRITE
    beq @c_bank1_nowrite
    txa
    and #BANK_SWITCH_COMMON_LC_READ_RAM
    beq @c_bank1_rom_write
    sta $C08B
    sta $C08B
    bra @c_mark_prewrite_on
@c_bank1_rom_write:
    sta $C089
    sta $C089
    bra @c_mark_prewrite_on
@c_bank1_nowrite:
    txa
    and #BANK_SWITCH_COMMON_LC_READ_RAM
    beq @c_bank1_rom_nowrite
    sta $C088
    bra @c_mark_prewrite_off
@c_bank1_rom_nowrite:
    sta $C08A
    bra @c_mark_prewrite_off

@c_bank2:
    txa
    and #BANK_SWITCH_COMMON_LC_WRITE
    beq @c_bank2_nowrite
    txa
    and #BANK_SWITCH_COMMON_LC_READ_RAM
    beq @c_bank2_rom_write
    sta $C083
    sta $C083
    bra @c_mark_prewrite_on
@c_bank2_rom_write:
    sta $C081
    sta $C081
    bra @c_mark_prewrite_on
@c_bank2_nowrite:
    txa
    and #BANK_SWITCH_COMMON_LC_READ_RAM
    beq @c_bank2_rom_nowrite
    sta $C080
    bra @c_mark_prewrite_off
@c_bank2_rom_nowrite:
    sta $C082
    bra @c_mark_prewrite_off

@c_mark_prewrite_on:
    lda BANK_SWITCH_EXT_STATE
    ora #BANK_SWITCH_EXT_LC_PREWRITE
    sta BANK_SWITCH_EXT_STATE
    bra @c_after_lc

@c_mark_prewrite_off:
    lda BANK_SWITCH_EXT_STATE
    and #($FF ^ BANK_SWITCH_EXT_LC_PREWRITE)
    sta BANK_SWITCH_EXT_STATE

@c_after_lc:
    txa
    and #BANK_SWITCH_COMMON_RAMRD
    beq @c_ramrd_off
    sta $C003
    bra @c_ramwrt_check
@c_ramrd_off:
    sta $C002

@c_ramwrt_check:
    txa
    and #BANK_SWITCH_COMMON_RAMWRT
    beq @c_ramwrt_off
    sta $C005
    bra @c_altzp_check
@c_ramwrt_off:
    sta $C004

@c_altzp_check:
    txa
    and #BANK_SWITCH_COMMON_ALTZP
    beq @c_want_main

    tya
    bmi @c_return_aux

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

@c_return_aux:
    lda #$00
    sta $C009
    rts

@c_want_main:
    tya
    bpl @c_return_main

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

@c_return_main:
    rts

bank_switch_common_tramp_end:

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

    lda #$00
    sta $C002
    sta $C004

    ldx #$00
@copy_extended_loop:
    lda bank_switch_extended_tramp_start,x
    sta BANK_SWITCH_TRAMP,x
    inx
    cpx #(bank_switch_extended_tramp_end - bank_switch_extended_tramp_start)
    bne @copy_extended_loop

    lda #$00
    sta $C005
    ldx #$00
@copy_extended_aux_loop:
    lda bank_switch_extended_tramp_start,x
    sta BANK_SWITCH_TRAMP,x
    inx
    cpx #(bank_switch_extended_tramp_end - bank_switch_extended_tramp_start)
    bne @copy_extended_aux_loop
    lda #$00
    sta $C004

    ldx BANK_SWITCH_EXT_STATE
    jmp BANK_SWITCH_TRAMP

; bank_switch_extended_tramp_start
; Args:      X = BANK_SWITCH_EXT_STATE, Y = original $C016 read
; Clobbers:  A, X, Y, flags, $FC in main ZP
; Returns:   RTS directly to the original caller
bank_switch_extended_tramp_start:
    txa
    and #BANK_SWITCH_EXT_INTCXROM
    beq @e_intcxrom_off
    sta $C007
    bra @e_intc8_check
@e_intcxrom_off:
    sta $C006

@e_intc8_check:
    txa
    and #BANK_SWITCH_EXT_INTC8ROM
    beq @e_intc8_off
    sta $C00A
    bit $C300
    bra @e_slotc3rom_check
@e_intc8_off:
    sta $CFFF

@e_slotc3rom_check:
    txa
    and #BANK_SWITCH_EXT_SLOTC3ROM
    beq @e_slotc3rom_off
    sta $C00B
    bra @e_prewrite_check
@e_slotc3rom_off:
    sta $C00A

@e_prewrite_check:
    lda #$00
    sta $C002
    sta $C004

    lda BANK_SWITCH_COMMON_STATE
    and #BANK_SWITCH_COMMON_LC_WRITE
    bne @e_force_prewrite_on

    txa
    and #BANK_SWITCH_EXT_LC_PREWRITE
    beq @e_clear_prewrite

    lda BANK_SWITCH_COMMON_STATE
    and #(BANK_SWITCH_COMMON_LC_BANK1 | BANK_SWITCH_COMMON_LC_READ_RAM)
    cmp #(BANK_SWITCH_COMMON_LC_BANK1 | BANK_SWITCH_COMMON_LC_READ_RAM)
    beq @e_bank1_ram_prewrite
    cmp #BANK_SWITCH_COMMON_LC_BANK1
    beq @e_bank1_rom_prewrite
    cmp #BANK_SWITCH_COMMON_LC_READ_RAM
    beq @e_bank2_ram_prewrite
    sta $C081
    bra @e_after_prewrite
@e_bank1_ram_prewrite:
    sta $C08B
    bra @e_after_prewrite
@e_bank1_rom_prewrite:
    sta $C089
    bra @e_after_prewrite
@e_bank2_ram_prewrite:
    sta $C083
    bra @e_after_prewrite

@e_clear_prewrite:
    lda BANK_SWITCH_COMMON_STATE
    and #(BANK_SWITCH_COMMON_LC_BANK1 | BANK_SWITCH_COMMON_LC_READ_RAM)
    cmp #(BANK_SWITCH_COMMON_LC_BANK1 | BANK_SWITCH_COMMON_LC_READ_RAM)
    beq @e_bank1_ram_clear
    cmp #BANK_SWITCH_COMMON_LC_BANK1
    beq @e_bank1_rom_clear
    cmp #BANK_SWITCH_COMMON_LC_READ_RAM
    beq @e_bank2_ram_clear
    sta $C082
    bra @e_after_prewrite
@e_bank1_ram_clear:
    sta $C088
    bra @e_after_prewrite
@e_bank1_rom_clear:
    sta $C08A
    bra @e_after_prewrite
@e_bank2_ram_clear:
    sta $C080
    bra @e_after_prewrite

@e_force_prewrite_on:
    lda BANK_SWITCH_EXT_STATE
    ora #BANK_SWITCH_EXT_LC_PREWRITE
    sta BANK_SWITCH_EXT_STATE

@e_after_prewrite:
    lda BANK_SWITCH_COMMON_STATE
    and #BANK_SWITCH_COMMON_RAMRD
    beq @e_ramrd_off
    sta $C003
    bra @e_ramwrt_check
@e_ramrd_off:
    sta $C002

@e_ramwrt_check:
    lda BANK_SWITCH_COMMON_STATE
    and #BANK_SWITCH_COMMON_RAMWRT
    beq @e_ramwrt_off
    sta $C005
    bra @e_an0_check
@e_ramwrt_off:
    sta $C004

@e_an0_check:
    txa
    and #BANK_SWITCH_EXT_AN0
    beq @e_an0_off
    sta $C059
    bra @e_an1_check
@e_an0_off:
    sta $C058

@e_an1_check:
    txa
    and #BANK_SWITCH_EXT_AN1
    beq @e_an1_off
    sta $C05B
    bra @e_an2_check
@e_an1_off:
    sta $C05A

@e_an2_check:
    txa
    and #BANK_SWITCH_EXT_AN2
    beq @e_an2_off
    sta $C05D
    bra @e_restore_altzp
@e_an2_off:
    sta $C05C

@e_restore_altzp:
    tya
    bpl @e_done
    lda #$00
    sta $C009
@e_done:
    rts

bank_switch_extended_tramp_end:

.endif

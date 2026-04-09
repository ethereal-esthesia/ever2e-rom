; Apple IIe ROM checksum helper recovered from `IIe_ROMSUM_F800FFFF.bin`.
;
; The original binary is 64 bytes:
; - 4-byte DOS 3.3-style BLOAD header: load address $0800, length $003C
; - 60-byte machine-code routine beginning at $0800
;
; Behavior:
; - `romsum_range` sums bytes from an arbitrary inclusive start/end range.
; - `romsum_range_banked` applies a requested ROM/language-card banking mode,
;   computes the checksum, then restores the previously tracked banking mode.
; - `romsum_bank_push_state` / `romsum_bank_restore_from_stack` use one byte
;   on the stack for save/restore, so callers can also pull/reuse the saved
;   mode manually if needed.
; - `romsum_f800ffff` is a convenience wrapper for the original $F800-$FFFF case.
; - Prints a newline, then the high byte and low byte in hex via monitor ROM.
; - Prints a trailing newline and returns with RTS.
;
; Clobbers: A, Y, flags, ROMSUM_* zero-page slots from `bank_switch.inc`

.include "bank_switch.inc"

MON_CROUT = $FD8E
MON_PRBYTE = $FDDA

ROMSUM_BANK_ROM = $00
ROMSUM_BANK_LC_BANK2 = $01
ROMSUM_BANK_LC_BANK1 = $02

LC_BANK2_READ = $C080
LC_ROM_READ = $C082
LC_BANK1_READ = $C088

; romsum_bank_apply_a
; Args:      A = ROMSUM_BANK_* mode
; Clobbers:  A, flags, $06
; Returns:   RTS
romsum_bank_apply_a:
    sta ROMSUM_BANK_STATE
    cmp #ROMSUM_BANK_LC_BANK2
    beq @bank2
    cmp #ROMSUM_BANK_LC_BANK1
    beq @bank1

@rom:
    bit LC_ROM_READ
    rts

@bank2:
    bit LC_BANK2_READ
    rts

@bank1:
    bit LC_BANK1_READ
    rts

; romsum_bank_request_a
; Args:      A = ROMSUM_BANK_* mode for the next `romsum_range_banked`
; Clobbers:  A, flags, $07
; Returns:   RTS
romsum_bank_request_a:
    sta ROMSUM_BANK_REQUEST
    rts

; romsum_bank_push_state
; Args:      none
; Clobbers:  A
; Returns:   saved mode pushed on stack, RTS
romsum_bank_push_state:
    lda ROMSUM_BANK_STATE
    pha
    rts

; romsum_bank_restore_from_stack
; Args:      none
; Clobbers:  A, flags, $06
; Returns:   RTS
romsum_bank_restore_from_stack:
    pla
    jmp romsum_bank_apply_a

; romsum_range
; Args:      $00/$01 = start address, $04/$05 = end address (inclusive)
; Clobbers:  A, Y, flags, $00-$05
; Returns:   $02/$03 = 16-bit checksum, RTS
romsum_range:
    lda #$00
    sta ROMSUM_SUM_LO
    sta ROMSUM_SUM_HI

@sum_loop:
    ldy #$00
    lda (ROMSUM_PTR_LO),y
    clc
    adc ROMSUM_SUM_LO
    sta ROMSUM_SUM_LO
    lda ROMSUM_SUM_HI
    adc #$00
    sta ROMSUM_SUM_HI

    lda ROMSUM_PTR_LO
    cmp ROMSUM_END_LO
    bne @advance_ptr
    lda ROMSUM_PTR_HI
    cmp ROMSUM_END_HI
    beq @print_sum

@advance_ptr:
    inc ROMSUM_PTR_LO
    bne @sum_loop
    inc ROMSUM_PTR_HI
    jmp @sum_loop

@print_sum:
    jsr MON_CROUT
    lda ROMSUM_SUM_HI
    jsr MON_PRBYTE
    lda ROMSUM_SUM_LO
    jsr MON_PRBYTE
    jsr MON_CROUT
    rts

; romsum_range_banked
; Args:      $00/$01 = start address, $04/$05 = end address (inclusive)
;            $07 = requested ROMSUM_BANK_* mode
; Clobbers:  A, Y, flags, $00-$07
; Returns:   $02/$03 = 16-bit checksum, RTS
romsum_range_banked:
    jsr romsum_bank_push_state
    lda ROMSUM_BANK_REQUEST
    jsr romsum_bank_apply_a
    jsr romsum_range
    jmp romsum_bank_restore_from_stack

; romsum_f800ffff
; Args:      none
; Clobbers:  A, Y, flags, $00-$07
; Returns:   $02/$03 = 16-bit checksum, RTS
romsum_f800ffff:
    lda #$00
    sta ROMSUM_PTR_LO
    lda #$F8
    sta ROMSUM_PTR_HI
    lda #$00
    sta ROMSUM_END_LO
    lda #$FF
    sta ROMSUM_END_HI
    jmp romsum_range

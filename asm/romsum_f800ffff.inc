; Apple IIe ROM checksum helper recovered from `IIe_ROMSUM_F800FFFF.bin`.
;
; The original binary is 64 bytes:
; - 4-byte DOS 3.3-style BLOAD header: load address $0800, length $003C
; - 60-byte machine-code routine beginning at $0800
;
; Behavior:
; - `romsum_compute_range` sums bytes from an arbitrary inclusive start/end range.
; - `romsum_print_sum` prints the current 16-bit checksum in hex (no newline).
; - `romsum_range` is a convenience wrapper that computes then prints on its own line.
; - `romsum_f800ffff` is a convenience wrapper for the original $F800-$FFFF case.
; - `romsum_range` prints a newline, then the high byte and low byte in hex,
;   then a trailing newline.
;
; Clobbers: A, Y, flags, shared ZP scratch aliases `ROMSUM_*` from
;           `bank_switch.inc`

.include "bank_switch.inc"

ROMSUM_PTR_LO       = ZP_SCRATCH_0
ROMSUM_PTR_HI       = ZP_SCRATCH_1
ROMSUM_SUM_LO       = ZP_SCRATCH_2
ROMSUM_SUM_HI       = ZP_SCRATCH_3
ROMSUM_END_LO       = ZP_SCRATCH_4
ROMSUM_END_HI       = ZP_SCRATCH_5

MON_CROUT = $FD8E
MON_PRBYTE = $FDDA

; romsum_compute_range
; Args:      $00/$01 = start address, $04/$05 = end address (inclusive)
; Clobbers:  A, Y, flags, $00-$05
; Returns:   $02/$03 = 16-bit checksum, RTS
romsum_compute_range_tramp_start:
romsum_compute_range:
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
    beq @done

@advance_ptr:
    inc ROMSUM_PTR_LO
    bne @sum_loop
    inc ROMSUM_PTR_HI
    bra @sum_loop

@done:
    rts
romsum_compute_range_tramp_end:

; romsum_print_sum
; Args:      none
; Clobbers:  A
; Returns:   RTS
romsum_print_sum:
    lda ROMSUM_SUM_HI
    jsr MON_PRBYTE
    lda ROMSUM_SUM_LO
    jsr MON_PRBYTE
    rts

; romsum_range
; Args:      $00/$01 = start address, $04/$05 = end address (inclusive)
; Clobbers:  A, Y, flags, $00-$05
; Returns:   $02/$03 = 16-bit checksum, RTS
romsum_range:
    jsr MON_CROUT
    jsr romsum_compute_range
    jsr romsum_print_sum
    jmp MON_CROUT

; romsum_f800ffff
; Args:      none
; Clobbers:  A, Y, flags, $00-$05
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

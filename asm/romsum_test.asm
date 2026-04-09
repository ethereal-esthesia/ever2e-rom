; Standalone checksum test harness for `romsum_range`.
;
; Edit `TEST_START` / `TEST_END` to choose the inclusive address range.
; Set `TEST_BANK_MODE` to one of:
; - `ROMSUM_BANK_ROM`
; - `ROMSUM_BANK_LC_BANK1`
; - `ROMSUM_BANK_LC_BANK2`
;
; The helper tracks the current bank mode, saves it, applies the requested
; mode for the checksum, then restores the saved mode afterward.
; On reset, the routine computes the checksum, prints it via monitor ROM,
; then idles forever so the result stays visible.

.setcpu "65C02"

TEST_START = $F800
TEST_END   = $FFFF

.segment "CODE"
.include "romsum_f800ffff.inc"

TEST_BANK_MODE = ROMSUM_BANK_ROM

reset:
    lda #ROMSUM_BANK_ROM
    jsr romsum_bank_apply_a

    lda #<TEST_START
    sta ROMSUM_PTR_LO
    lda #>TEST_START
    sta ROMSUM_PTR_HI

    lda #<TEST_END
    sta ROMSUM_END_LO
    lda #>TEST_END
    sta ROMSUM_END_HI

    lda #TEST_BANK_MODE
    jsr romsum_bank_request_a
    jsr romsum_range_banked

@idle:
    jmp @idle

; nmi
; Args:      none
; Clobbers:  none
; Returns:   RTI
nmi:
    rti

; irq
; Args:      none
; Clobbers:  none
; Returns:   RTI
irq:
    rti

.segment "VECTORS"
    .word nmi
    .word reset
    .word irq

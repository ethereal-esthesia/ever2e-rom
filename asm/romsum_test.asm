; Standalone checksum test harness for `romsum_range`.
;
; Edit `TEST_START` / `TEST_END` to choose the inclusive address range.
; Set `TEST_COMMON_BANK_STATE` to the common bank-routing state you want active
; before the checksum runs. Banking is managed by `bank_switch.inc`; the checksum
; helper itself now just sums the active address window.
; On reset, the routine computes the checksum, prints it via monitor ROM,
; then idles forever so the result stays visible.

.setcpu "65C02"

TEST_START = $F800
TEST_END   = $FFFF

.segment "CODE"
.include "display.inc"
.include "romsum_f800ffff.inc"

TEST_COMMON_BANK_STATE = BANK_SWITCH_COMMON_RESET_STATE

reset:
    lda #BANK_SWITCH_EXT_RESET_STATE
    jsr bank_switch_apply_extended_state
    lda #TEST_COMMON_BANK_STATE
    jsr bank_switch_apply_common_state
    lda #DISPLAY_RESET_STATE
    jsr display_apply_state
    lda #INVFLG_NORMAL
    sta INVFLG
    jsr display_text_clear_visible
    jsr display_text_home

    lda #<TEST_START
    sta ROMSUM_PTR_LO
    lda #>TEST_START
    sta ROMSUM_PTR_HI

    lda #<TEST_END
    sta ROMSUM_END_LO
    lda #>TEST_END
    sta ROMSUM_END_HI

    jsr romsum_range

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

.setcpu "65C02"

.segment "CODE"

.include "bank_switch.inc"
.include "display.inc"
.include "rng.inc"
.include "romsum_f800ffff.inc"

TEST_OUTPUT_BUFFER = $0C20
TEST_COMMON_STATE_WORK_BYTE = $0C25
TEST_RESULT_OFFSET_WORK_BYTE = $0C28
TEST_HEX_TEMPORARY_BYTE      = $0C29

TEST_EXPECTED_MAIN_WINDOW_SUM_LO  = $0C30
TEST_EXPECTED_MAIN_WINDOW_SUM_HI  = $0C31
TEST_EXPECTED_AUX_WINDOW_SUM_LO   = $0C32
TEST_EXPECTED_AUX_WINDOW_SUM_HI   = $0C33
TEST_EXPECTED_ZP_MAIN_SUM_LO      = $0C34
TEST_EXPECTED_ZP_MAIN_SUM_HI      = $0C35
TEST_EXPECTED_ZP_AUX_SUM_LO       = $0C36
TEST_EXPECTED_ZP_AUX_SUM_HI       = $0C37
TEST_EXPECTED_LC_ROM_SUM_LO       = $0C38
TEST_EXPECTED_LC_ROM_SUM_HI       = $0C39
TEST_EXPECTED_LC_BANK2_SUM_LO     = $0C3A
TEST_EXPECTED_LC_BANK2_SUM_HI     = $0C3B
TEST_EXPECTED_LC_BANK1_SUM_LO     = $0C3C
TEST_EXPECTED_LC_BANK1_SUM_HI     = $0C3D

TEST_LABEL_PADDING_FOR_11_CHARACTER_FIELD = 2
TEST_LABEL_PADDING_FOR_10_CHARACTER_FIELD = 3
TEST_LABEL_PADDING_FOR_9_CHARACTER_FIELD  = 4
TEST_LABEL_PADDING_FOR_8_CHARACTER_FIELD  = 5
TEST_LABEL_PADDING_FOR_6_CHARACTER_FIELD  = 7

TEST_WORKER_TRAMP = $0C40
ROMSUM_WORKER_TRAMP = $0D00

TEST_RNG_SUM_WORK_LO         = ZP_SCRATCH_2
TEST_RNG_SUM_WORK_HI         = ZP_SCRATCH_3
TEST_TARGET_POINTER_LO       = ZP_SCRATCH_4
TEST_TARGET_POINTER_HI       = ZP_SCRATCH_5

; Shared test windows:
; - $0C00-$0C0F: main/aux RAM write+read routing
; - $0010-$001F: main/aux ZP routing through ALTZP
; - $D000-$D00F: ROM/bank2/bank1 language-card routing
MAIN_TEST_START = $0C00
MAIN_TEST_END   = $0C0F
ZP_TEST_START   = $0010
ZP_TEST_END     = $001F
LC_TEST_START   = $D000
LC_TEST_END     = $D00F

reset:
    jsr baseline_all
    jsr display_text_clear_visible
    jsr display_text_home
    jsr write_test_patterns
    jsr display_text_clear_visible
    jsr display_text_home

    lda #<msg_banner
    ldx #>msg_banner
    ldy #$00
    jsr print_string_ax

    lda #<msg_status
    ldx #>msg_status
    ldy #$00
    jsr print_string_ax
    jsr run_status_tests

    lda #<msg_checksum
    ldx #>msg_checksum
    ldy #$00
    jsr print_string_ax
    jsr run_checksum_tests

@idle:
    jmp @idle

baseline_all:
    lda #$00
    jsr bank_switch_apply_extended_state
    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state
    lda #DISPLAY_RESET_STATE
    jsr display_apply_state
    lda #INVFLG_NORMAL
    sta INVFLG
    jsr display_text_home
    rts

write_test_patterns:
    jsr baseline_all
    jsr set_lc_range
    jsr romsum_compute_range
    lda ROMSUM_SUM_LO
    sta TEST_EXPECTED_LC_ROM_SUM_LO
    lda ROMSUM_SUM_HI
    sta TEST_EXPECTED_LC_ROM_SUM_HI
    jsr rng_seed_default

    lda #<MAIN_TEST_START
    sta TEST_TARGET_POINTER_LO
    lda #>MAIN_TEST_START
    sta TEST_TARGET_POINTER_HI
    jsr fill_rng_target_16_bytes_and_sum
    lda TEST_RNG_SUM_WORK_LO
    sta TEST_EXPECTED_MAIN_WINDOW_SUM_LO
    lda TEST_RNG_SUM_WORK_HI
    sta TEST_EXPECTED_MAIN_WINDOW_SUM_HI

    lda #BANK_SWITCH_COMMON_RAMWRT
    jsr bank_switch_apply_common_state
    lda #<MAIN_TEST_START
    sta TEST_TARGET_POINTER_LO
    lda #>MAIN_TEST_START
    sta TEST_TARGET_POINTER_HI
    jsr fill_rng_target_16_bytes_and_sum
    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state
    lda TEST_RNG_SUM_WORK_LO
    sta TEST_EXPECTED_AUX_WINDOW_SUM_LO
    lda TEST_RNG_SUM_WORK_HI
    sta TEST_EXPECTED_AUX_WINDOW_SUM_HI

    lda #<ZP_TEST_START
    sta TEST_TARGET_POINTER_LO
    lda #>ZP_TEST_START
    sta TEST_TARGET_POINTER_HI
    jsr fill_rng_target_16_bytes_and_sum
    lda TEST_RNG_SUM_WORK_LO
    sta TEST_EXPECTED_ZP_MAIN_SUM_LO
    lda TEST_RNG_SUM_WORK_HI
    sta TEST_EXPECTED_ZP_MAIN_SUM_HI

    lda #BANK_SWITCH_COMMON_ALTZP
    jsr bank_switch_apply_common_state
    lda #<ZP_TEST_START
    sta TEST_TARGET_POINTER_LO
    lda #>ZP_TEST_START
    sta TEST_TARGET_POINTER_HI
    jsr fill_rng_target_16_bytes_and_sum
    lda TEST_RNG_SUM_WORK_LO
    sta TEST_EXPECTED_ZP_AUX_SUM_LO
    lda TEST_RNG_SUM_WORK_HI
    sta TEST_EXPECTED_ZP_AUX_SUM_HI
    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state

    lda #BANK_SWITCH_COMMON_LC_WRITE
    jsr bank_switch_apply_common_state
    lda #<LC_TEST_START
    sta TEST_TARGET_POINTER_LO
    lda #>LC_TEST_START
    sta TEST_TARGET_POINTER_HI
    jsr fill_rng_target_16_bytes_and_sum
    lda TEST_RNG_SUM_WORK_LO
    sta TEST_EXPECTED_LC_BANK2_SUM_LO
    lda TEST_RNG_SUM_WORK_HI
    sta TEST_EXPECTED_LC_BANK2_SUM_HI

    lda #(BANK_SWITCH_COMMON_LC_BANK1 | BANK_SWITCH_COMMON_LC_WRITE)
    jsr bank_switch_apply_common_state
    lda #<LC_TEST_START
    sta TEST_TARGET_POINTER_LO
    lda #>LC_TEST_START
    sta TEST_TARGET_POINTER_HI
    jsr fill_rng_target_16_bytes_and_sum
    lda TEST_RNG_SUM_WORK_LO
    sta TEST_EXPECTED_LC_BANK1_SUM_LO
    lda TEST_RNG_SUM_WORK_HI
    sta TEST_EXPECTED_LC_BANK1_SUM_HI

    jmp baseline_all

run_status_tests:
    jsr common_status_bank1
    jsr common_status_lcram
    jsr common_status_ramrd
    jsr common_status_ramwrt
    jsr common_status_altzp
    jsr common_track_lcwrite
    jsr ext_status_intcxrom
    jsr ext_track_intc8rom
    jsr ext_track_prewrite
    jsr ext_track_an0
    jsr ext_track_an1
    jsr ext_track_an2
    rts

run_checksum_tests:
    jsr checksum_main_aux
    jsr checksum_zp_altzp
    jsr checksum_lc_rom_bank2
    jsr checksum_lc_bank2_bank1
    rts

common_status_bank1:
    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state
    lda $C011
    sta TEST_OUTPUT_BUFFER
    lda #BANK_SWITCH_COMMON_LC_BANK1
    jsr bank_switch_common_set
    lda $C011
    sta TEST_OUTPUT_BUFFER+1
    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state
    lda #<msg_bank1_status
    ldx #>msg_bank1_status
    ldy #TEST_LABEL_PADDING_FOR_10_CHARACTER_FIELD
    jmp print_status_line_ax

common_status_lcram:
    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state
    lda $C012
    sta TEST_OUTPUT_BUFFER
    lda #BANK_SWITCH_COMMON_LC_READ_RAM
    ldx #$01
    jsr run_lcram_status_worker
    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state
    lda #<msg_lcram_status
    ldx #>msg_lcram_status
    ldy #TEST_LABEL_PADDING_FOR_10_CHARACTER_FIELD
    jmp print_status_line_ax

common_status_ramrd:
    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state
    lda $C013
    sta TEST_OUTPUT_BUFFER
    lda #BANK_SWITCH_COMMON_RAMRD
    jsr bank_switch_common_set
    lda $C013
    sta TEST_OUTPUT_BUFFER+1
    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state
    lda #<msg_ramrd_status
    ldx #>msg_ramrd_status
    ldy #TEST_LABEL_PADDING_FOR_10_CHARACTER_FIELD
    jmp print_status_line_ax

common_status_ramwrt:
    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state
    lda $C014
    sta TEST_OUTPUT_BUFFER
    lda #BANK_SWITCH_COMMON_RAMWRT
    jsr bank_switch_common_set
    lda $C014
    sta TEST_OUTPUT_BUFFER+1
    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state
    lda #<msg_ramwrt_status
    ldx #>msg_ramwrt_status
    ldy #TEST_LABEL_PADDING_FOR_11_CHARACTER_FIELD
    jmp print_status_line_ax

common_status_altzp:
    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state
    lda $C016
    sta TEST_OUTPUT_BUFFER
    lda #BANK_SWITCH_COMMON_ALTZP
    jsr bank_switch_common_set
    lda $C016
    sta TEST_OUTPUT_BUFFER+1
    lda #BANK_SWITCH_COMMON_ALTZP
    jsr bank_switch_common_reset
    lda #<msg_altzp_status
    ldx #>msg_altzp_status
    ldy #TEST_LABEL_PADDING_FOR_10_CHARACTER_FIELD
    jmp print_status_line_ax

common_track_lcwrite:
    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state
    lda BANK_SWITCH_COMMON_STATE
    sta TEST_OUTPUT_BUFFER
    lda #BANK_SWITCH_COMMON_LC_WRITE
    jsr bank_switch_common_set
    lda BANK_SWITCH_COMMON_STATE
    sta TEST_OUTPUT_BUFFER+1
    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state
    lda #<msg_lcwrite_track
    ldx #>msg_lcwrite_track
    ldy #TEST_LABEL_PADDING_FOR_10_CHARACTER_FIELD
    jmp print_status_line_ax

ext_status_intcxrom:
    lda #$00
    jsr bank_switch_apply_extended_state
    lda $C015
    sta TEST_OUTPUT_BUFFER
    lda #BANK_SWITCH_EXT_INTCXROM
    jsr bank_switch_extended_set
    lda $C015
    sta TEST_OUTPUT_BUFFER+1
    lda #$00
    jsr bank_switch_apply_extended_state
    lda #<msg_intcxrom_status
    ldx #>msg_intcxrom_status
    ldy #TEST_LABEL_PADDING_FOR_10_CHARACTER_FIELD
    jmp print_status_line_ax

ext_track_intc8rom:
    lda #$00
    jsr bank_switch_apply_extended_state
    lda BANK_SWITCH_EXT_STATE
    sta TEST_OUTPUT_BUFFER
    lda #BANK_SWITCH_EXT_INTC8ROM
    jsr bank_switch_extended_set
    lda BANK_SWITCH_EXT_STATE
    sta TEST_OUTPUT_BUFFER+1
    lda #$00
    jsr bank_switch_apply_extended_state
    lda #<msg_intc8rom_track
    ldx #>msg_intc8rom_track
    ldy #TEST_LABEL_PADDING_FOR_8_CHARACTER_FIELD
    jmp print_status_line_ax

ext_track_prewrite:
    lda #$00
    jsr bank_switch_apply_extended_state
    lda BANK_SWITCH_EXT_STATE
    sta TEST_OUTPUT_BUFFER
    lda #BANK_SWITCH_EXT_LC_PREWRITE
    jsr bank_switch_extended_set
    lda BANK_SWITCH_EXT_STATE
    sta TEST_OUTPUT_BUFFER+1
    lda #$00
    jsr bank_switch_apply_extended_state
    lda #<msg_prewrite_track
    ldx #>msg_prewrite_track
    ldy #TEST_LABEL_PADDING_FOR_9_CHARACTER_FIELD
    jmp print_status_line_ax

ext_track_an0:
    lda #$00
    jsr bank_switch_apply_extended_state
    lda BANK_SWITCH_EXT_STATE
    sta TEST_OUTPUT_BUFFER
    lda #BANK_SWITCH_EXT_AN0
    jsr bank_switch_extended_set
    lda BANK_SWITCH_EXT_STATE
    sta TEST_OUTPUT_BUFFER+1
    lda #$00
    jsr bank_switch_apply_extended_state
    lda #<msg_an0_track
    ldx #>msg_an0_track
    ldy #TEST_LABEL_PADDING_FOR_6_CHARACTER_FIELD
    jmp print_status_line_ax

ext_track_an1:
    lda #$00
    jsr bank_switch_apply_extended_state
    lda BANK_SWITCH_EXT_STATE
    sta TEST_OUTPUT_BUFFER
    lda #BANK_SWITCH_EXT_AN1
    jsr bank_switch_extended_set
    lda BANK_SWITCH_EXT_STATE
    sta TEST_OUTPUT_BUFFER+1
    lda #$00
    jsr bank_switch_apply_extended_state
    lda #<msg_an1_track
    ldx #>msg_an1_track
    ldy #TEST_LABEL_PADDING_FOR_6_CHARACTER_FIELD
    jmp print_status_line_ax

ext_track_an2:
    lda #$00
    jsr bank_switch_apply_extended_state
    lda BANK_SWITCH_EXT_STATE
    sta TEST_OUTPUT_BUFFER
    lda #BANK_SWITCH_EXT_AN2
    jsr bank_switch_extended_set
    lda BANK_SWITCH_EXT_STATE
    sta TEST_OUTPUT_BUFFER+1
    lda #$00
    jsr bank_switch_apply_extended_state
    lda #<msg_an2_track
    ldx #>msg_an2_track
    ldy #TEST_LABEL_PADDING_FOR_6_CHARACTER_FIELD
    jmp print_status_line_ax

checksum_main_aux:
    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state
    jsr set_main_range
    jsr romsum_compute_range
    lda ROMSUM_SUM_HI
    sta TEST_OUTPUT_BUFFER
    lda ROMSUM_SUM_LO
    sta TEST_OUTPUT_BUFFER+1

    lda #BANK_SWITCH_COMMON_RAMRD
    jsr bank_switch_common_set
    jsr set_main_range
    jsr romsum_compute_range
    lda ROMSUM_SUM_HI
    sta TEST_OUTPUT_BUFFER+2
    lda ROMSUM_SUM_LO
    sta TEST_OUTPUT_BUFFER+3

    lda #BANK_SWITCH_COMMON_RAMRD
    jsr bank_switch_common_reset
    lda TEST_EXPECTED_MAIN_WINDOW_SUM_HI
    sta TEST_OUTPUT_BUFFER+4
    lda TEST_EXPECTED_MAIN_WINDOW_SUM_LO
    sta TEST_OUTPUT_BUFFER+5
    lda TEST_EXPECTED_AUX_WINDOW_SUM_HI
    sta TEST_OUTPUT_BUFFER+6
    lda TEST_EXPECTED_AUX_WINDOW_SUM_LO
    sta TEST_OUTPUT_BUFFER+7

    lda #<msg_main_aux_checksum
    ldx #>msg_main_aux_checksum
    ldy #TEST_LABEL_PADDING_FOR_9_CHARACTER_FIELD
    jmp print_checksum_line_ax

checksum_zp_altzp:
    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state
    jsr set_zp_range
    jsr romsum_compute_range
    lda ROMSUM_SUM_HI
    sta TEST_OUTPUT_BUFFER
    lda ROMSUM_SUM_LO
    sta TEST_OUTPUT_BUFFER+1

    lda #BANK_SWITCH_COMMON_ALTZP
    jsr bank_switch_common_set
    jsr set_zp_range
    jsr romsum_compute_range
    lda ROMSUM_SUM_HI
    sta TEST_OUTPUT_BUFFER+2
    lda ROMSUM_SUM_LO
    sta TEST_OUTPUT_BUFFER+3
    lda TEST_EXPECTED_ZP_MAIN_SUM_HI
    sta TEST_OUTPUT_BUFFER+4
    lda TEST_EXPECTED_ZP_MAIN_SUM_LO
    sta TEST_OUTPUT_BUFFER+5
    lda TEST_EXPECTED_ZP_AUX_SUM_HI
    sta TEST_OUTPUT_BUFFER+6
    lda TEST_EXPECTED_ZP_AUX_SUM_LO
    sta TEST_OUTPUT_BUFFER+7

    lda #BANK_SWITCH_COMMON_ALTZP
    jsr bank_switch_common_reset
    lda #<msg_altzp_checksum
    ldx #>msg_altzp_checksum
    ldy #TEST_LABEL_PADDING_FOR_8_CHARACTER_FIELD
    jmp print_checksum_line_ax

checksum_lc_rom_bank2:
    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state
    jsr set_lc_range
    jsr romsum_compute_range
    lda ROMSUM_SUM_HI
    sta TEST_OUTPUT_BUFFER
    lda ROMSUM_SUM_LO
    sta TEST_OUTPUT_BUFFER+1

    lda #BANK_SWITCH_COMMON_LC_READ_RAM
    ldx #$02
    jsr run_lc_checksum_worker
    lda TEST_EXPECTED_LC_ROM_SUM_HI
    sta TEST_OUTPUT_BUFFER+4
    lda TEST_EXPECTED_LC_ROM_SUM_LO
    sta TEST_OUTPUT_BUFFER+5
    lda TEST_EXPECTED_LC_BANK2_SUM_HI
    sta TEST_OUTPUT_BUFFER+6
    lda TEST_EXPECTED_LC_BANK2_SUM_LO
    sta TEST_OUTPUT_BUFFER+7

    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state
    lda #<msg_lc_rom_bank2_checksum
    ldx #>msg_lc_rom_bank2_checksum
    ldy #TEST_LABEL_PADDING_FOR_9_CHARACTER_FIELD
    jmp print_checksum_line_ax

checksum_lc_bank2_bank1:
    lda #BANK_SWITCH_COMMON_LC_READ_RAM
    ldx #$00
    jsr run_lc_checksum_worker

    lda #BANK_SWITCH_COMMON_LC_READ_RAM | BANK_SWITCH_COMMON_LC_BANK1
    ldx #$02
    jsr run_lc_checksum_worker
    lda TEST_EXPECTED_LC_BANK2_SUM_HI
    sta TEST_OUTPUT_BUFFER+4
    lda TEST_EXPECTED_LC_BANK2_SUM_LO
    sta TEST_OUTPUT_BUFFER+5
    lda TEST_EXPECTED_LC_BANK1_SUM_HI
    sta TEST_OUTPUT_BUFFER+6
    lda TEST_EXPECTED_LC_BANK1_SUM_LO
    sta TEST_OUTPUT_BUFFER+7

    lda #BANK_SWITCH_COMMON_RESET_STATE
    jsr bank_switch_apply_common_state
    lda #<msg_lc_bank2_bank1_checksum
    ldx #>msg_lc_bank2_bank1_checksum
    ldy #TEST_LABEL_PADDING_FOR_8_CHARACTER_FIELD
    jmp print_checksum_line_ax

set_main_range:
    lda #<MAIN_TEST_START
    sta ROMSUM_PTR_LO
    lda #>MAIN_TEST_START
    sta ROMSUM_PTR_HI
    lda #<MAIN_TEST_END
    sta ROMSUM_END_LO
    lda #>MAIN_TEST_END
    sta ROMSUM_END_HI
    rts

set_zp_range:
    lda #<ZP_TEST_START
    sta ROMSUM_PTR_LO
    lda #>ZP_TEST_START
    sta ROMSUM_PTR_HI
    lda #<ZP_TEST_END
    sta ROMSUM_END_LO
    lda #>ZP_TEST_END
    sta ROMSUM_END_HI
    rts

set_lc_range:
    lda #<LC_TEST_START
    sta ROMSUM_PTR_LO
    lda #>LC_TEST_START
    sta ROMSUM_PTR_HI
    lda #<LC_TEST_END
    sta ROMSUM_END_LO
    lda #>LC_TEST_END
    sta ROMSUM_END_HI
    rts

run_lcram_status_worker:
    sta TEST_COMMON_STATE_WORK_BYTE
    stx TEST_RESULT_OFFSET_WORK_BYTE
    ldx #$00
@copy_status_loop:
    lda lc_status_worker_start,x
    sta TEST_WORKER_TRAMP,x
    inx
    cpx #(lc_status_worker_end - lc_status_worker_start)
    bne @copy_status_loop
    jsr TEST_WORKER_TRAMP
    rts

run_lc_checksum_worker:
    sta TEST_COMMON_STATE_WORK_BYTE
    stx TEST_RESULT_OFFSET_WORK_BYTE
    ldx #$00
@copy_checksum_loop:
    lda lc_checksum_worker_start,x
    sta TEST_WORKER_TRAMP,x
    inx
    cpx #(lc_checksum_worker_end - lc_checksum_worker_start)
    bne @copy_checksum_loop
    jsr TEST_WORKER_TRAMP
    rts

lc_status_worker_start:
    lda TEST_COMMON_STATE_WORK_BYTE
    jsr bank_switch_apply_common_state
    ldx TEST_RESULT_OFFSET_WORK_BYTE
    lda $C012
    sta TEST_OUTPUT_BUFFER,x
    ldx #BANK_SWITCH_COMMON_RESET_STATE
    lda $C016
    tay
    lda #$00
    sta $C008
    stx BANK_SWITCH_COMMON_STATE
    jsr BANK_SWITCH_TRAMP
    rts
lc_status_worker_end:

lc_checksum_worker_start:
    ldx #$00
@copy_romsum_loop:
    lda romsum_compute_range_tramp_start,x
    sta ROMSUM_WORKER_TRAMP,x
    inx
    cpx #(romsum_compute_range_tramp_end - romsum_compute_range_tramp_start)
    bne @copy_romsum_loop

    lda TEST_COMMON_STATE_WORK_BYTE
    jsr bank_switch_apply_common_state

    lda #<LC_TEST_START
    sta ROMSUM_PTR_LO
    lda #>LC_TEST_START
    sta ROMSUM_PTR_HI
    lda #<LC_TEST_END
    sta ROMSUM_END_LO
    lda #>LC_TEST_END
    sta ROMSUM_END_HI
    jsr ROMSUM_WORKER_TRAMP

    ldx TEST_RESULT_OFFSET_WORK_BYTE
    lda ROMSUM_SUM_HI
    sta TEST_OUTPUT_BUFFER,x
    inx
    lda ROMSUM_SUM_LO
    sta TEST_OUTPUT_BUFFER,x

    ldx #BANK_SWITCH_COMMON_RESET_STATE
    lda $C016
    tay
    lda #$00
    sta $C008
    stx BANK_SWITCH_COMMON_STATE
    jsr BANK_SWITCH_TRAMP
    rts
lc_checksum_worker_end:

fill_rng_target_16_bytes_and_sum:
    stz TEST_RNG_SUM_WORK_LO
    stz TEST_RNG_SUM_WORK_HI
    ldy #$00
@fill_loop:
    jsr rng_next_mixed_a
    sta (TEST_TARGET_POINTER_LO),y
    clc
    adc TEST_RNG_SUM_WORK_LO
    sta TEST_RNG_SUM_WORK_LO
    lda TEST_RNG_SUM_WORK_HI
    adc #$00
    sta TEST_RNG_SUM_WORK_HI
    iny
    cpy #$10
    bne @fill_loop
    rts

print_status_line_ax:
    pha
    phx
    jsr text_begin_line
    plx
    pla
    jsr display_text_write_null_terminated_string_ax_y_padded
    lda TEST_OUTPUT_BUFFER
    jsr text_put_hex_byte_a
    lda #' '
    jsr text_putc_a
    lda TEST_OUTPUT_BUFFER+1
    jsr text_put_hex_byte_a
    inc CV
    rts

print_checksum_line_ax:
    pha
    phx
    jsr text_begin_line
    plx
    pla
    jsr display_text_write_null_terminated_string_ax_y_padded
    lda TEST_OUTPUT_BUFFER
    jsr text_put_hex_byte_a
    lda TEST_OUTPUT_BUFFER+1
    jsr text_put_hex_byte_a
    lda #' '
    jsr text_putc_a
    lda TEST_OUTPUT_BUFFER+2
    jsr text_put_hex_byte_a
    lda TEST_OUTPUT_BUFFER+3
    jsr text_put_hex_byte_a
    lda #' '
    jsr text_putc_a
    lda TEST_OUTPUT_BUFFER+4
    jsr text_put_hex_byte_a
    lda TEST_OUTPUT_BUFFER+5
    jsr text_put_hex_byte_a
    lda #' '
    jsr text_putc_a
    lda TEST_OUTPUT_BUFFER+6
    jsr text_put_hex_byte_a
    lda TEST_OUTPUT_BUFFER+7
    jsr text_put_hex_byte_a
    inc CV
    rts

print_string_ax:
    pha
    phx
    jsr text_begin_line
    plx
    pla
    jsr display_text_write_null_terminated_string_ax_y_padded
    inc CV
    rts

text_begin_line:
    stz CH
    rts

text_put_hex_byte_a:
    sta TEST_HEX_TEMPORARY_BYTE
    lsr a
    lsr a
    lsr a
    lsr a
    jsr text_put_hex_nibble_a
    lda TEST_HEX_TEMPORARY_BYTE
    and #$0F
    jmp text_put_hex_nibble_a

text_put_hex_nibble_a:
    cmp #$0A
    bcc @digit
    clc
    adc #$37
    jmp text_putc_a
@digit:
    clc
    adc #$30
    jmp text_putc_a

text_putc_a:
    phy
    ldx CH
    ldy CV
    jsr display_text_write_char_clipped
    ply
    inc CH
    rts

msg_banner:
    .byte "BANK SWITCH CHECKSUM TEST",0
msg_status:
    .byte "STATUS/TRACK OFF ON",0
msg_checksum:
    .byte "CHECKSUM OFF ON EOFF EON",0
msg_bank1_status:
    .byte "BANK1 C011",0
msg_lcram_status:
    .byte "LCRAM C012",0
msg_ramrd_status:
    .byte "RAMRD C013",0
msg_ramwrt_status:
    .byte "RAMWRT C014",0
msg_altzp_status:
    .byte "ALTZP C016",0
msg_lcwrite_track:
    .byte "LCWRITE FD",0
msg_intcxrom_status:
    .byte "INTCX C015",0
msg_intc8rom_track:
    .byte "INTC8 FC",0
msg_prewrite_track:
    .byte "PREWRT FC",0
msg_an0_track:
    .byte "AN0 FC",0
msg_an1_track:
    .byte "AN1 FC",0
msg_an2_track:
    .byte "AN2 FC",0
msg_main_aux_checksum:
    .byte "RAM $0C00",0
msg_altzp_checksum:
    .byte "ZP $0010",0
msg_lc_rom_bank2_checksum:
    .byte "LC ROM/B2",0
msg_lc_bank2_bank1_checksum:
    .byte "LC B2/B1",0

nmi:
    rti

irq:
    rti

.segment "VECTORS"
    .word nmi
    .word reset
    .word irq

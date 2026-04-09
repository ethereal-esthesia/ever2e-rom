.setcpu "65C02"

.segment "CODE"

.include "bank_switch.inc"
.include "display.inc"
.include "rng.inc"

RNG_SEQUENCE_TEST_SEED_LO              = $00
RNG_SEQUENCE_TEST_SEED_HI              = $00
RNG_SEQUENCE_TEST_FIRST_BYTE_0        = $0C20
RNG_SEQUENCE_TEST_FIRST_BYTE_1        = $0C21
RNG_SEQUENCE_TEST_FIRST_BYTE_2        = $0C22
RNG_SEQUENCE_TEST_FIRST_BYTE_3        = $0C23
RNG_SEQUENCE_TEST_WINDOW_BYTE_1       = $0C24
RNG_SEQUENCE_TEST_WINDOW_BYTE_2       = $0C25
RNG_SEQUENCE_TEST_WINDOW_BYTE_3       = $0C26
RNG_SEQUENCE_TEST_COUNT_LO            = $0C27
RNG_SEQUENCE_TEST_COUNT_HI            = $0C28
RNG_SEQUENCE_TEST_HEX_TEMPORARY_BYTE  = $0C29
RNG_SEQUENCE_TEST_SUM_LO              = $0C2A
RNG_SEQUENCE_TEST_SUM_MID             = $0C2B
RNG_SEQUENCE_TEST_SUM_HI              = $0C2C
RNG_SEQUENCE_TEST_SUM_TOP             = $0C2D
RNG_SEQUENCE_TEST_SAMPLE_COUNT_LO     = $0C2E
RNG_SEQUENCE_TEST_SAMPLE_COUNT_HI     = $0C2F
RNG_SEQUENCE_TEST_AVERAGE_BYTE        = $0C30

reset:
    jsr rng_sequence_test_baseline_display
    jsr display_text_clear_visible
    jsr display_text_home

    lda #RNG_SEQUENCE_TEST_SEED_LO
    ldx #RNG_SEQUENCE_TEST_SEED_HI
    jsr rng_seed_ax

    jsr rng_next_mixed_a
    sta RNG_SEQUENCE_TEST_FIRST_BYTE_0

    jsr rng_next_mixed_a
    sta RNG_SEQUENCE_TEST_FIRST_BYTE_1
    sta RNG_SEQUENCE_TEST_WINDOW_BYTE_1

    jsr rng_next_mixed_a
    sta RNG_SEQUENCE_TEST_FIRST_BYTE_2
    sta RNG_SEQUENCE_TEST_WINDOW_BYTE_2

    jsr rng_next_mixed_a
    sta RNG_SEQUENCE_TEST_FIRST_BYTE_3
    sta RNG_SEQUENCE_TEST_WINDOW_BYTE_3

    stz RNG_SEQUENCE_TEST_COUNT_LO
    stz RNG_SEQUENCE_TEST_COUNT_HI
    stz RNG_SEQUENCE_TEST_SUM_LO
    stz RNG_SEQUENCE_TEST_SUM_MID
    stz RNG_SEQUENCE_TEST_SUM_HI
    stz RNG_SEQUENCE_TEST_SUM_TOP
    lda RNG_SEQUENCE_TEST_FIRST_BYTE_0
    jsr rng_sequence_test_add_byte_a_to_sum
    lda RNG_SEQUENCE_TEST_FIRST_BYTE_1
    jsr rng_sequence_test_add_byte_a_to_sum
    lda RNG_SEQUENCE_TEST_FIRST_BYTE_2
    jsr rng_sequence_test_add_byte_a_to_sum
    lda RNG_SEQUENCE_TEST_FIRST_BYTE_3
    jsr rng_sequence_test_add_byte_a_to_sum

@search_loop:
    jsr rng_next_mixed_a
    pha
    jsr rng_sequence_test_increment_count
    pla
    pha
    jsr rng_sequence_test_add_byte_a_to_sum
    pla

    ldx RNG_SEQUENCE_TEST_WINDOW_BYTE_1
    cpx RNG_SEQUENCE_TEST_FIRST_BYTE_0
    bne @remember_current
    ldx RNG_SEQUENCE_TEST_WINDOW_BYTE_2
    cpx RNG_SEQUENCE_TEST_FIRST_BYTE_1
    bne @remember_current
    ldx RNG_SEQUENCE_TEST_WINDOW_BYTE_3
    cpx RNG_SEQUENCE_TEST_FIRST_BYTE_2
    bne @remember_current
    cmp RNG_SEQUENCE_TEST_FIRST_BYTE_3
    beq @print_results

@remember_current:
    ldx RNG_SEQUENCE_TEST_WINDOW_BYTE_2
    stx RNG_SEQUENCE_TEST_WINDOW_BYTE_1
    ldx RNG_SEQUENCE_TEST_WINDOW_BYTE_3
    stx RNG_SEQUENCE_TEST_WINDOW_BYTE_2
    sta RNG_SEQUENCE_TEST_WINDOW_BYTE_3
    jmp @search_loop

@print_results:
    jsr rng_sequence_test_compute_average
    jsr display_text_clear_visible
    jsr display_text_home

    lda #<rng_sequence_test_banner_text
    ldx #>rng_sequence_test_banner_text
    ldy #$00
    jsr rng_sequence_test_print_string_ax
    inc CV

    lda #<rng_sequence_test_seed_text
    ldx #>rng_sequence_test_seed_text
    ldy #$00
    jsr rng_sequence_test_print_string_ax
    lda #RNG_SEQUENCE_TEST_SEED_HI
    jsr rng_sequence_test_text_put_hex_byte_a
    lda #RNG_SEQUENCE_TEST_SEED_LO
    jsr rng_sequence_test_text_put_hex_byte_a
    inc CV

    lda #<rng_sequence_test_first_pair_text
    ldx #>rng_sequence_test_first_pair_text
    ldy #$00
    jsr rng_sequence_test_print_string_ax
    lda RNG_SEQUENCE_TEST_FIRST_BYTE_0
    jsr rng_sequence_test_text_put_hex_byte_a
    lda #' '
    jsr rng_sequence_test_text_putc_a
    lda RNG_SEQUENCE_TEST_FIRST_BYTE_1
    jsr rng_sequence_test_text_put_hex_byte_a
    lda #' '
    jsr rng_sequence_test_text_putc_a
    lda RNG_SEQUENCE_TEST_FIRST_BYTE_2
    jsr rng_sequence_test_text_put_hex_byte_a
    lda #' '
    jsr rng_sequence_test_text_putc_a
    lda RNG_SEQUENCE_TEST_FIRST_BYTE_3
    jsr rng_sequence_test_text_put_hex_byte_a
    inc CV

    lda #<rng_sequence_test_count_text
    ldx #>rng_sequence_test_count_text
    ldy #$00
    jsr rng_sequence_test_print_string_ax
    lda RNG_SEQUENCE_TEST_COUNT_HI
    jsr rng_sequence_test_text_put_hex_byte_a
    lda RNG_SEQUENCE_TEST_COUNT_LO
    jsr rng_sequence_test_text_put_hex_byte_a
    inc CV

    lda #<rng_sequence_test_average_text
    ldx #>rng_sequence_test_average_text
    ldy #$00
    jsr rng_sequence_test_print_string_ax
    lda RNG_SEQUENCE_TEST_AVERAGE_BYTE
    jsr rng_sequence_test_text_put_hex_byte_a
    inc CV

@idle:
    jmp @idle

rng_sequence_test_baseline_display:
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

rng_sequence_test_increment_count:
    inc RNG_SEQUENCE_TEST_COUNT_LO
    bne @done
    inc RNG_SEQUENCE_TEST_COUNT_HI
@done:
    rts

rng_sequence_test_add_byte_a_to_sum:
    clc
    adc RNG_SEQUENCE_TEST_SUM_LO
    sta RNG_SEQUENCE_TEST_SUM_LO
    lda RNG_SEQUENCE_TEST_SUM_MID
    adc #$00
    sta RNG_SEQUENCE_TEST_SUM_MID
    lda RNG_SEQUENCE_TEST_SUM_HI
    adc #$00
    sta RNG_SEQUENCE_TEST_SUM_HI
    lda RNG_SEQUENCE_TEST_SUM_TOP
    adc #$00
    sta RNG_SEQUENCE_TEST_SUM_TOP
    rts

rng_sequence_test_compute_average:
    lda RNG_SEQUENCE_TEST_COUNT_LO
    clc
    adc #$04
    sta RNG_SEQUENCE_TEST_SAMPLE_COUNT_LO
    lda RNG_SEQUENCE_TEST_COUNT_HI
    adc #$00
    sta RNG_SEQUENCE_TEST_SAMPLE_COUNT_HI
    stz RNG_SEQUENCE_TEST_AVERAGE_BYTE

@divide_loop:
    lda RNG_SEQUENCE_TEST_SUM_TOP
    bne @subtract_sample_count
    lda RNG_SEQUENCE_TEST_SUM_HI
    bne @subtract_sample_count
    lda RNG_SEQUENCE_TEST_SUM_MID
    cmp RNG_SEQUENCE_TEST_SAMPLE_COUNT_HI
    bcc @average_done
    bne @subtract_sample_count
    lda RNG_SEQUENCE_TEST_SUM_LO
    cmp RNG_SEQUENCE_TEST_SAMPLE_COUNT_LO
    bcc @average_done

@subtract_sample_count:
    lda RNG_SEQUENCE_TEST_SUM_LO
    sec
    sbc RNG_SEQUENCE_TEST_SAMPLE_COUNT_LO
    sta RNG_SEQUENCE_TEST_SUM_LO
    lda RNG_SEQUENCE_TEST_SUM_MID
    sbc RNG_SEQUENCE_TEST_SAMPLE_COUNT_HI
    sta RNG_SEQUENCE_TEST_SUM_MID
    lda RNG_SEQUENCE_TEST_SUM_HI
    sbc #$00
    sta RNG_SEQUENCE_TEST_SUM_HI
    lda RNG_SEQUENCE_TEST_SUM_TOP
    sbc #$00
    sta RNG_SEQUENCE_TEST_SUM_TOP
    inc RNG_SEQUENCE_TEST_AVERAGE_BYTE
    bne @divide_loop

@average_done:
    rts

rng_sequence_test_print_string_ax:
    jsr rng_sequence_test_text_begin_line
    jsr display_text_write_null_terminated_string_ax_y_padded
    rts

rng_sequence_test_text_begin_line:
    stz CH
    rts

rng_sequence_test_text_put_hex_byte_a:
    sta RNG_SEQUENCE_TEST_HEX_TEMPORARY_BYTE
    lsr a
    lsr a
    lsr a
    lsr a
    jsr rng_sequence_test_text_put_hex_nibble_a
    lda RNG_SEQUENCE_TEST_HEX_TEMPORARY_BYTE
    and #$0F
    jmp rng_sequence_test_text_put_hex_nibble_a

rng_sequence_test_text_put_hex_nibble_a:
    cmp #$0A
    bcc @digit
    clc
    adc #$37
    jmp rng_sequence_test_text_putc_a
@digit:
    clc
    adc #$30
    jmp rng_sequence_test_text_putc_a

rng_sequence_test_text_putc_a:
    phy
    ldx CH
    ldy CV
    jsr display_text_write_char_clipped
    ply
    inc CH
    rts

rng_sequence_test_banner_text:
    .byte "RNG SEQUENCE TEST",0
rng_sequence_test_seed_text:
    .byte "SEED ",0
rng_sequence_test_first_pair_text:
    .byte "FIRST4 ",0
rng_sequence_test_count_text:
    .byte "COUNT ",0
rng_sequence_test_average_text:
    .byte "AVG ",0

nmi:
    rti

irq:
    rti

.segment "VECTORS"
    .word nmi
    .word reset
    .word irq

; ever2e-rom main entry
; Assembled with ca65 for 65C02/G65SC02-compatible opcodes.

.setcpu "65C02"

.import softswitch_reset_optimal
.import clear_ram_unrolled_fast
.import babeep
.import ensure_g65sc02_or_beep_loop

.segment "CODE"
.include "display.inc"
.include "dhgr.inc"
.include "get_key.inc"
.include "reset.inc"

; Keep the demo cursor in regular main RAM so $00-$0F stays available as
; shared routine scratch.
PIXEL_X = $0C00
PIXEL_Y = $0C01

main_loop:
    jsr main_show_banner
    stz PIXEL_X
    stz PIXEL_Y

@wait_key:
    jsr get_key_blocking
    and #$7F

    ; Accept only 0-9 / A-F / a-f and convert to color nibble 0-15.
    cmp #'0'
    bcc @wait_key
    cmp #'9'+1
    bcc @digit
    cmp #'A'
    bcc @check_lower
    cmp #'F'+1
    bcc @upper_hex

@check_lower:
    cmp #'a'
    bcc @wait_key
    cmp #'f'+1
    bcs @wait_key

    sec
    sbc #'a'
    clc
    adc #$0A
    jmp @plot

@upper_hex:
    sec
    sbc #'A'
    clc
    adc #$0A
    jmp @plot

@digit:
    sec
    sbc #'0'

@plot:
    pha
    lda #$01
    jsr dhgr_display
    pla
    ldx PIXEL_X
    ldy PIXEL_Y
    clc                 ; page 1
    jsr dhgr_plot
    
    ; Inner loop: advance X across 0..139.
    inc PIXEL_X
    lda PIXEL_X
    cmp #$8C
    bcc @wait_key

    ; Outer loop: when X wraps, advance Y across 0..191.
    stz PIXEL_X
    inc PIXEL_Y
    lda PIXEL_Y
    cmp #$C0
    bcc @wait_key
    stz PIXEL_Y
    jmp @wait_key

; main_show_banner
; Args:      none
; Clobbers:  A, X, Y, flags
; Returns:   RTS
main_show_banner:
    jsr display_text_clear_visible
    ldx #$00
@banner_loop:
    lda main_banner,x
    beq @done
    phx
    ldy #$00
    jsr display_text_write_char_normal_clipped
    plx
    inx
    bne @banner_loop
@done:
    rts

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

main_banner:
    .byte "Ever2e",0

.segment "VECTORS"
    .word nmi
    .word reset
    .word irq

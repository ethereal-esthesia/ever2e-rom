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
MAIN_DEMO_PIXEL_X_COORDINATE = $0C00
MAIN_DEMO_PIXEL_Y_COORDINATE = $0C01

main_loop:
    jsr main_show_startup_banner
    stz MAIN_DEMO_PIXEL_X_COORDINATE
    stz MAIN_DEMO_PIXEL_Y_COORDINATE

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
    ldx MAIN_DEMO_PIXEL_X_COORDINATE
    ldy MAIN_DEMO_PIXEL_Y_COORDINATE
    clc                 ; page 1
    jsr dhgr_plot
    
    ; Inner loop: advance X across 0..139.
    inc MAIN_DEMO_PIXEL_X_COORDINATE
    lda MAIN_DEMO_PIXEL_X_COORDINATE
    cmp #$8C
    bcc @wait_key

    ; Outer loop: when X wraps, advance Y across 0..191.
    stz MAIN_DEMO_PIXEL_X_COORDINATE
    inc MAIN_DEMO_PIXEL_Y_COORDINATE
    lda MAIN_DEMO_PIXEL_Y_COORDINATE
    cmp #$C0
    bcc @wait_key
    stz MAIN_DEMO_PIXEL_Y_COORDINATE
    jmp @wait_key

; main_show_startup_banner
; Args:      none
; Clobbers:  A, X, Y, flags
; Returns:   RTS
main_show_startup_banner:
    lda #INVFLG_INVERSE
    sta INVFLG
    jsr display_text_clear_visible
    jsr display_text_home
    lda #<MAIN_STARTUP_BANNER_TEXT
    ldx #>MAIN_STARTUP_BANNER_TEXT
    ldy #$00
    jsr display_text_write_null_terminated_string_ax_y_padded
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

MAIN_STARTUP_BANNER_TEXT:
    .byte "Ever2e",0

.segment "VECTORS"
    .word nmi
    .word reset
    .word irq

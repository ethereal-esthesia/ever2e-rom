; Double-hires helpers (reset/minimal version).
;
; This pass keeps display setup + Y/page lookup and adds:
; - x/7 + x%7 phase mapping
; - white nibble OR into stream bits [x*4 .. x*4+3]

.setcpu "65C02"

.export dhgr_display
.export dhgr_plot
.export dhgr_plot_page1
.export dhgr_plot_page2

DHGR_PTR_LO   = $FB
DHGR_PTR_HI   = $FC
DHGR_PHASE    = $FD
DHGR_PAGE_HI  = $FE
DHGR_TMP      = $FA

; dhgr_display
; Args:      A = page (1 or 2)
; Clobbers:  A, flags
; Returns:   RTS
dhgr_display:
    sta $C000           ; 80STORE off
    sta $C057           ; HIRES on
    sta $C05E           ; AN3 clear
    sta $C052           ; MIXED off
    sta $C00D           ; 80COL on

    cmp #$02
    beq @page2
    sta $C054           ; page 1
    jmp @done
@page2:
    sta $C055           ; page 2
@done:
    sta $C050           ; GRAPHICS on
    rts

dhgr_plot_page1:
    clc
    jmp dhgr_plot

dhgr_plot_page2:
    sec
    jmp dhgr_plot

; dhgr_plot
; Args:      A = color (0-15), X = x (0-139), Y = y (0-191)
;            C clear => page 1, C set => page 2
; Clobbers:  A, X, Y, flags, $FB-$FE (ZP scratch), RAMRD/RAMWRT soft-switches
; Returns:   RTS
; Notes:
; - Computes stream group/phase using x/7 and x%7.
; - ORs white nibble (0x0F) into stream bit range [x*4 .. x*4+3].
; - Then XORs with inverse-color masks so final nibble becomes requested color.
dhgr_plot:
    ; Consume page-select carry immediately.
    and #$0F
    eor #$0F            ; inverse color nibble (~color & 0x0F)
    pha                 ; save inverse color until page-high is consumed
    lda #$01
    adc #$00            ; C=0 -> A=1 (page1), C=1 -> A=2 (page2)
    lsr a
    ror a               ; 1 -> $80, 2 -> $00
    ror a               ; 1 -> $40, 2 -> $80
    lsr a               ; 1 -> $20, 2 -> $40
    sta DHGR_PAGE_HI    ; page base high-byte contribution ($20/$40)

    ; Clip coordinates to valid DHGR ranges.
    cpy #$C0
    bcc :+
    ldy #$BF
:
    cpx #$8C
    bcc :+
    ldx #$8B
:

    ; Row base address from Y lookup table (page 1 base), then page adjust.
    lda dhgr_row_addr_lo,y
    sta DHGR_PTR_LO
    lda dhgr_row_addr_hi5,y
    ora DHGR_PAGE_HI
    sta DHGR_PTR_HI
    pla
    sta DHGR_PAGE_HI    ; reuse: inverse color nibble

    ; Lookup x/7-derived group offset and x%7 phase (faster than divide loop).
    lda dhgr_x_group_off,x
    clc
    adc DHGR_PTR_LO
    sta DHGR_PTR_LO
    bcc :+
    inc DHGR_PTR_HI
:
    lda dhgr_x_phase,x
    sta DHGR_PHASE

    ; First stream byte OR.
    ldx DHGR_PHASE
    lda dhgr_phase_stream_idx,x
    tax
    jsr dhgr_load_stream_byte
    ldx DHGR_PHASE
    ora dhgr_phase_first_mask,x
    sta DHGR_TMP
    ; Build color-phase index: idx = (inverse_color * 7) + phase.
    lda DHGR_PAGE_HI
    tay
    lda dhgr_mul7,y
    clc
    adc DHGR_PHASE
    tay
    lda dhgr_inv_first_mask,y
    eor DHGR_TMP
    pha
    ldx DHGR_PHASE
    lda dhgr_phase_stream_idx,x
    tax
    pla
    jsr dhgr_store_stream_byte

    ; Crossing phases (1,3,5) OR into second stream byte.
    ldx DHGR_PHASE
    lda dhgr_phase_crosses,x
    beq @done

    lda dhgr_phase_stream_idx,x
    clc
    adc #$01
    tax
    jsr dhgr_load_stream_byte
    ldx DHGR_PHASE
    ora dhgr_phase_second_mask,x
    sta DHGR_TMP
    ; Rebuild color-phase index for second-byte XOR mask.
    lda DHGR_PAGE_HI
    tay
    lda dhgr_mul7,y
    clc
    adc DHGR_PHASE
    tay
    lda dhgr_inv_second_mask,y
    eor DHGR_TMP
    pha
    ldx DHGR_PHASE
    lda dhgr_phase_stream_idx,x
    clc
    adc #$01
    tax
    pla
    jsr dhgr_store_stream_byte

@done:
    sta $C002           ; RAMRD -> main
    sta $C004           ; RAMWRT -> main
    rts

; X = stream index 0..3
; Returns: A = stream byte
; Clobbers: A, X, Y
; Uses: DHGR_PTR_LO/DHGR_PTR_HI

dhgr_load_stream_byte:
    txa
    lsr a               ; byte offset: 0 for idx 0/1, 1 for idx 2/3
    tay
    txa
    and #$01
    bne @load_main
    sta $C003           ; RAMRD -> auxiliary
    lda (DHGR_PTR_LO),y
    sta $C002           ; RAMRD -> main
    rts
@load_main:
    sta $C002           ; RAMRD -> main
    lda (DHGR_PTR_LO),y
    rts

; X = stream index 0..3, A = new stream byte
; Clobbers: A, X, Y
; Uses: DHGR_PTR_LO/DHGR_PTR_HI

dhgr_store_stream_byte:
    pha
    txa
    lsr a               ; byte offset: 0 for idx 0/1, 1 for idx 2/3
    tay
    txa
    and #$01
    bne @store_main
    sta $C005           ; RAMWRT -> auxiliary
    pla
    sta (DHGR_PTR_LO),y
    sta $C004           ; RAMWRT -> main
    rts
@store_main:
    sta $C004           ; RAMWRT -> main
    pla
    sta (DHGR_PTR_LO),y
    rts

dhgr_row_addr_lo:
.repeat 192, I
    .byte <($2000 + ((I & $07) * $0400) + ((I & $38) * $0010) + ((I >> 6) * $0028))
.endrepeat

dhgr_row_addr_hi5:
.repeat 192, I
    .byte (>($2000 + ((I & $07) * $0400) + ((I & $38) * $0010) + ((I >> 6) * $0028))) & $1F
.endrepeat

; Phase tables indexed by (x % 7), using 28-bit stream model (4 bits/pixel).
dhgr_phase_stream_idx:
    .byte $00, $00, $01, $01, $02, $02, $03
dhgr_phase_first_mask:
    .byte $0F, $70, $1E, $60, $3C, $40, $78
dhgr_phase_crosses:
    .byte $00, $01, $00, $01, $00, $01, $00
dhgr_phase_second_mask:
    .byte $00, $01, $00, $03, $00, $07, $00

; X mapping tables for 0..139
dhgr_x_phase:
.repeat 140, I
    .byte (I - ((I / 7) * 7))
.endrepeat

dhgr_x_group_off:
.repeat 140, I
    .byte ((I / 7) * 2)
.endrepeat

; Multiply table for 4-bit nibble: value * 7.
dhgr_mul7:
    .byte $00, $07, $0E, $15, $1C, $23, $2A, $31
    .byte $38, $3F, $46, $4D, $54, $5B, $62, $69

; XOR masks indexed by (inverse_color * 7 + phase).
; Each entry maps the 4-bit inverse color nibble into the current phase's stream bits.
dhgr_inv_first_mask:
.repeat 16, C
    .repeat 7, P
        .if P = 0
            .byte (C & $0F)
        .elseif P = 1
            .byte ((C & $07) << 4)
        .elseif P = 2
            .byte ((C << 1) & $1E)
        .elseif P = 3
            .byte ((C & $03) << 5)
        .elseif P = 4
            .byte ((C << 2) & $3C)
        .elseif P = 5
            .byte ((C & $01) << 6)
        .else
            .byte ((C << 3) & $78)
        .endif
    .endrepeat
.endrepeat

dhgr_inv_second_mask:
.repeat 16, C
    .repeat 7, P
        .if P = 1
            .byte ((C >> 3) & $01)
        .elseif P = 3
            .byte ((C >> 2) & $03)
        .elseif P = 5
            .byte ((C >> 1) & $07)
        .else
            .byte $00
        .endif
    .endrepeat
.endrepeat

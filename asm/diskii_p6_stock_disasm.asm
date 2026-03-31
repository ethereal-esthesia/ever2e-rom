; da65 V2.18 - N/A
; Created:    2026-03-21 20:59:43
; Input file: ROMS/DISKII_P6_STOCK.rom
; Page:       1
;
; Enriched local notes:
; - This is a raw disassembly of the 256-byte Disk II P6 ROM.
; - Entry-point fingerprints used by our tests:
;   $00, $5C, $5D, $A6, $BA, $CB, $D7.
; - ProDOS/controller fingerprint bytes ($00..$07):
;   A2 20 A0 00 A2 03 86 3C


        .setcpu "6502"

L0801           := $0801
LFCA8           := $FCA8
LFF58           := $FF58
;
; Zero-page usage in this ROM:
; $26/$27 = destination pointer (decode/write target)
; $2B     = slot-derived soft-switch base selector
; $3C     = scratch / nibble decode state / loop temp
; $3D/$41 = address/checksum-like running state
; $40     = compare latch for sync validation

; ---------------------------------------------------------------------------
; Offset $00: Boot entry / ProDOS-identifying signature region.
; ---------------------------------------------------------------------------
; Signature bytes ($00..$07): A2 20 A0 00 A2 03 86 3C
; Signature instruction sequence:
;   ldx #$20
;   ldy #$00
;   ldx #$03
;   stx $3C
        ldx     #$20
        ldy     #$00
        ldx     #$03
RO06:  stx     $3C
        txa
        asl     a
        bit     $3C
        beq     RO1E
        ora     $3C
        eor     #$FF
        and     #$7E
RO14:  bcs     RO1E
        lsr     a
        bne     RO14
        tya
        sta     $0356,x
        iny
RO1E:  inx
        bpl     RO06

; Warm-start handoff prep: read caller return info, derive slot, poke switches.
        jsr     LFF58
        tsx
        lda     $0100,x
        asl     a
        asl     a
        asl     a
        asl     a
        sta     $2B
        tax
        lda     $C08E,x
        lda     $C08C,x
        lda     $C08A,x
        lda     $C089,x
        ldy     #$50
RO3D:  lda     $C080,x
        tya
        and     #$03
        asl     a
        ora     $2B
        tax
        lda     $C081,x
        lda     #$56
        jsr     LFCA8
        dey
        bpl     RO3D

; Initialize decode destination and state.
        sta     $26
        sta     $3D
        sta     $41
        lda     #$08
        sta     $27

; ---------------------------------------------------------------------------
; Offset $5C: Sync prologue scanner.
; Wait for D5 AA 96 (or recovery marker AD), then branch to decode paths.
; ---------------------------------------------------------------------------
RO5C:  clc
RO5D:  php
RO5E:  lda     $C08C,x
        bpl     RO5E
RO63:  eor     #$D5
        bne     RO5E
RO67:  lda     $C08C,x
        bpl     RO67
        cmp     #$AA
        bne     RO63
        nop
RO71:  lda     $C08C,x
        bpl     RO71
        cmp     #$96
        beq     RO83
        plp
        bcc     RO5C
        eor     #$AD
        beq     ROA6
        bne     RO5C

; 3-byte predecode/validation loop.
RO83:  ldy     #$03
RO85:  sta     $40
RO87:  lda     $C08C,x
        bpl     RO87
        rol     a
        sta     $3C
RO8F:  lda     $C08C,x
        bpl     RO8F
        and     $3C
        dey
        bne     RO85
        plp
        cmp     $3D
        bne     RO5C
        lda     $40
        cmp     $41
        bne     RO5C
        bcs     RO5D

; ---------------------------------------------------------------------------
; Offset $A6: Decode stream into $0300 page.
; ---------------------------------------------------------------------------
ROA6:  ldy     #$56
ROA8:  sty     $3C
ROAA:  ldy     $C08C,x
        bpl     ROAA
        eor     $02D6,y
        ldy     $3C
        dey
        sta     $0300,y
        bne     ROA8

; ---------------------------------------------------------------------------
; Offset $BA: Decode stream into destination pointer ($26),Y.
; ---------------------------------------------------------------------------
ROBA:  sty     $3C
ROBC:  ldy     $C08C,x
        bpl     ROBC
        eor     $02D6,y
        ldy     $3C
        sta     ($26),y
        iny
        bne     ROBA

; ---------------------------------------------------------------------------
; Offset $CB: Decode tail / gate into pack loop.
; ---------------------------------------------------------------------------
ROCB:  ldy     $C08C,x
        bpl     ROCB
        eor     $02D6,y
ROD3:  bne     RO5C
        ldy     #$00

; ---------------------------------------------------------------------------
; Offset $D7: Bit-pack loop.
; Converts staged data in $0300 into final destination buffer, then loops until
; page/address limit, finally exits to monitor/BASIC entry chain.
; ---------------------------------------------------------------------------
ROD7:  ldx     #$56
ROD9:  dex
        bmi     ROD7
        lda     ($26),y
        lsr     $0300,x
        rol     a
        lsr     $0300,x
        rol     a
        sta     ($26),y
        iny
        bne     ROD9
        inc     $27
        inc     $3D
        lda     $3D
        cmp     $0800
        ldx     $2B
        bcc     ROD3
        jmp     L0801

; Padding/trailer.
        brk
        brk
        brk
        brk
        brk

.ifndef EVER2E_RNG_INC
EVER2E_RNG_INC = 1

; Shared hybrid PRNG helper.
;
; Design:
; - High byte: 8-bit Weyl sequence with odd increment for full 256-step cycle.
; - Low byte: 8-bit xorshift sequence seeded from the first Weyl output.
; - Public output: XOR of the Weyl and xorshift bytes, so callers do not see
;   the raw linear Weyl ramp directly.
; - Combined mixed-byte output cycle: lcm(256, 255) = 65,280 values.
;
; Call contract:
; - `rng_seed_ax`: caller provides a 16-bit seed with A = low byte, X = high byte.
;   - low byte seeds the Weyl state directly
;   - high byte is XORed with the first Weyl output to derive the xorshift seed
; - `rng_seed_weyl_a`: compatibility wrapper with X = 0.
; - `rng_seed_default`: uses 16-bit seed 0.
; - `rng_next_mixed_a`: returns A = (Weyl byte XOR xorshift byte).
;
; State:
; - Persistent RNG state lives in regular main RAM, not ZP, so the repo keeps
;   `$FC-$FE` as the only always-live shared ZP control bytes.

.setcpu "65C02"

.include "bank_switch.inc"

.export rng_seed_ax
.export rng_seed_weyl_a
.export rng_seed_default
.export rng_next_mixed_a

RNG_WEYL_STATE_MAIN_RAM      = $0C3E
RNG_XORSHIFT_STATE_MAIN_RAM  = $0C3F
RNG_XORSHIFT_WORK_BYTE       = ZP_SCRATCH_0
RNG_MIXED_OUTPUT_WORK_BYTE   = ZP_SCRATCH_1
RNG_WEYL_INCREMENT           = $9D

.segment "CODE"

rng_seed_default:
    lda #$00
    ldx #$00
    ; Fall through so the default seed path and explicit seed path stay identical.

rng_seed_ax:
    sta RNG_WEYL_STATE_MAIN_RAM
    phx
    jsr rng_next_weyl_byte_a
    bne :+
    lda #$01
:
    sta RNG_MIXED_OUTPUT_WORK_BYTE
    plx
    txa
    eor RNG_MIXED_OUTPUT_WORK_BYTE
    bne :+
    lda #$01
:
    sta RNG_XORSHIFT_STATE_MAIN_RAM
    rts

rng_seed_weyl_a:
    ldx #$00
    jmp rng_seed_ax

rng_next_mixed_a:
    jsr rng_next_weyl_byte_a
    sta RNG_MIXED_OUTPUT_WORK_BYTE
    jsr rng_next_xorshift_byte_a
    eor RNG_MIXED_OUTPUT_WORK_BYTE
    rts

rng_next_weyl_byte_a:
    clc
    lda RNG_WEYL_STATE_MAIN_RAM
    adc #RNG_WEYL_INCREMENT
    sta RNG_WEYL_STATE_MAIN_RAM
    rts

rng_next_xorshift_byte_a:
    lda RNG_XORSHIFT_STATE_MAIN_RAM
    sta RNG_XORSHIFT_WORK_BYTE

    asl a
    eor RNG_XORSHIFT_WORK_BYTE
    sta RNG_XORSHIFT_WORK_BYTE

    lda RNG_XORSHIFT_WORK_BYTE
    lsr a
    eor RNG_XORSHIFT_WORK_BYTE
    sta RNG_XORSHIFT_WORK_BYTE

    lda RNG_XORSHIFT_WORK_BYTE
    asl a
    asl a
    eor RNG_XORSHIFT_WORK_BYTE
    sta RNG_XORSHIFT_STATE_MAIN_RAM
    rts

.endif

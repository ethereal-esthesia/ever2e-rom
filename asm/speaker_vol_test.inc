; Speaker variable-interval tone + fixed NOP volume paths
; Inputs:
;   $FC tone interval control byte (pitch).
;   $FB low nibble volume path (0..15).
; Behavior:
;   - Clears stale keyboard strobe once at entry.
;   - Continuously generates square-wave output until key-down at $C000 bit 7.
;   - Returns with RTS when key-down is detected.
;   - Leaves strobe clear/consume behavior to BASIC.
; Interval model:
;   - $FC is clamped to [12..255].
;   - Delay loop uses DEX/BNE (5n-1 cycles), giving roughly 64..1274 cycle spacing.
; Volume model:
;   - 16 fixed paths from "toggle,toggle" through "toggle,NOPx15,toggle".
; Clobbers: A, X, flags

speaker_vol_test_slice:
    bit $C010               ; clear any stale keyboard strobe before tone loop
                           ; (new keypress will relatch while we run)
tone_loop:
    lda $C000               ; key-down sets bit 7
    bmi speaker_vol_test_done
    jsr speaker_tone_interval
    jmp tone_loop

speaker_vol_test_done:
    rts

speaker_tone_interval:
    lda $FB
    and #$0F
    tax
    jsr speaker_vol_dispatch

    lda $FC
    cmp #12                 ; floor ~64-cycle interval
    bcs :+
    lda #12
:
    tax
pitch_delay_loop:
    dex
    bne pitch_delay_loop
    rts

speaker_vol_dispatch:
    txa
    asl a
    tax
    jmp (speaker_vol_test_table,x)

.macro VOL_PATH name, nops_between
name:
    bit $C030               ; speaker toggle #1
    .repeat nops_between
        nop
    .endrep
    bit $C030               ; speaker toggle #2
    jmp speaker_vol_done
.endmacro

speaker_vol_done:
    rts

speaker_vol_test_table:
    .word speaker_vol_v0, speaker_vol_v1, speaker_vol_v2, speaker_vol_v3
    .word speaker_vol_v4, speaker_vol_v5, speaker_vol_v6, speaker_vol_v7
    .word speaker_vol_v8, speaker_vol_v9, speaker_vol_v10, speaker_vol_v11
    .word speaker_vol_v12, speaker_vol_v13, speaker_vol_v14, speaker_vol_v15

VOL_PATH speaker_vol_v0,  0
VOL_PATH speaker_vol_v1,  1
VOL_PATH speaker_vol_v2,  2
VOL_PATH speaker_vol_v3,  3
VOL_PATH speaker_vol_v4,  4
VOL_PATH speaker_vol_v5,  5
VOL_PATH speaker_vol_v6,  6
VOL_PATH speaker_vol_v7,  7
VOL_PATH speaker_vol_v8,  8
VOL_PATH speaker_vol_v9,  9
VOL_PATH speaker_vol_v10, 10
VOL_PATH speaker_vol_v11, 11
VOL_PATH speaker_vol_v12, 12
VOL_PATH speaker_vol_v13, 13
VOL_PATH speaker_vol_v14, 14
VOL_PATH speaker_vol_v15, 15

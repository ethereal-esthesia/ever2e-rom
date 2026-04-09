; Runtime guard for G65SC02/65C02-class CPU behavior.
; Probe uses PHX/PLX opcodes:
; - On 65C02-class cores, PHX pushes X (SP changes) and PLX restores.
; - If probe fails, call babeep forever.

.setcpu "65C02"

.import babeep

.export ensure_g65sc02_or_beep_loop

.segment "CODE"

; ensure_g65sc02_or_beep_loop
; Args:      none
; Clobbers:  A, X, flags, $00 (shared ZP scratch)
; Returns:   RTS on success; otherwise never returns (beep loop)
ensure_g65sc02_or_beep_loop:
    tsx
    stx $00          ; save SP baseline in ZP scratch

    ldx #$A5
    phx              ; 65C02 opcode
    tsx
    cpx $00
    beq @fail        ; SP unchanged => PHX did not execute as expected

    plx              ; 65C02 opcode
    cpx #$A5
    bne @fail
    rts

@fail:
    jsr babeep
    jmp @fail

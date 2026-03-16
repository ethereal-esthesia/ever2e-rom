; Blocking keyboard read helper.
;
; get_key_blocking
; Args:      none
; Clobbers:  A, flags
; Returns:   A = key byte from $C000 (high bit set)
; Notes:
; - Polls until a key is available.
; - Clears keyboard strobe via $C010 before returning.

get_key_blocking:
@poll:
    lda $C000           ; keyboard data ($80 set when key available)
    bpl @poll
    bit $C010           ; clear keyboard strobe
    rts

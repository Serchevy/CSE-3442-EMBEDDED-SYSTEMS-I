	.def wait3seconds

N .field 17142857

.thumb
.const

wait3seconds:
        LDR R1, N		;  1
LOOP:   SUB R1, R1, #1	;  N
        CBZ R1, END		; (N-1) + 3
        NOP				; (N-1)
        NOP				; (N-1)
        NOP				; (N-1)
        B LOOP			; 2(N-1)
END:    BX LR			; ------
						; 7N-2 = 120,000,000

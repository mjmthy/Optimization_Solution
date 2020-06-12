#define LOOP_UNROLL_FACTOR   100

	.syntax unified
	.text
	.arch armv7-a
	.fpu neon
	.global routine
	.type routine, %function
	.align 2
	.arm

routine:
	push        {r4-r12, lr}
	vpush       {q8-q15}

	mrrc	    15, 0, r4, r5, cr14 // store the start cycle,
					 // with r4 low 32bits &&
					 // r5 high 32bits
	adds	    r6, r4, r0     // store the end cycle,
	adc	    r7, r5, r1    // with r6 low 32bits &&
				    // r7 high 32bits
	vmov.u8     q12, #0xff
        vmov.u8     q13, #0xff
        vmov.u8     q14, #0xff
        vmov.u8     q15, #0xff

        adr         lr, 9f
        add         lr, lr, #1
        mov         r1, #(64 / 4)
        mov         r2, #(-64 / 4)

	b           0f

    .ltorg
9:
    .rept 64
        .long 0xffffffff
    .endr

    .balign 64
0:
    .rept LOOP_UNROLL_FACTOR
        vaba.u32    q8, q12, q13
        bne         1f
1:      vaba.u32    q9, q14, q15
        ldrne       r0, [lr, r1, lsl #2]!
        vaba.u32    q10, q12, q13
        bne         1f
1:      vaba.u32    q11, q14, q15
        ldrne       r0, [lr, r2, lsl #2]!
    .endr
	/*
         * Check if timeout via global counter
         */
	mrrc        15, 0, r8, r9, cr14
	cmp	    r7, r9
	bgt	    0b
	blt	    return
	cmp	    r6, r8 // if eq of high 32bits, check the low 32bits
	bgt	    0b
        //b           0b
return:
	vpop        {q8-q15}
        mov         r0, #0
        pop         {r4-r12, pc}

#define LOOP_UNROLL_FACTOR   100

	.cpu cortex-a53+fp+simd
	.text
	.align 2
	.global routine
	.type routine, %function

routine:
	stp	x29, x30, [sp, #-16]!
	stp	x9, x10, [sp, #-16]!
	stp	x11, x12, [sp, #-16]!
	stp	d8, d9, [sp, #-16]!
	stp	d10, d11, [sp, #-16]!
	stp	d12, d13, [sp, #-16]!
	stp	d14, d15, [sp, #-16]!

	mrs	x9, cntvct_el0 // x9 stores the start cycle
	/*
	 * Although a 64bit counter is available, we
	 * only use the low 56bits
	 */
	mov	x10, 0xFFFFFFFFFFFFFF
	cmp	x0, 0x0
	b.eq	0f
	add	x10, x9, x0    // x10 stores the end cycle

0:
	movi	v28.16b, #0xff
        movi	v29.16b, #0xff
        movi	v30.16b, #0xff
        movi	v31.16b, #0xff

	adr	x4, 9f
        add	x4, x4, #1
        mov	x1, #0
        mov	x2, #(64 / 4)

	b	0f

    .ltorg
9:
    .rept 64
        .long 0xffffffff
    .endr

    .balign 64
0:
    .rept LOOP_UNROLL_FACTOR
	uaba	v8.4s, v28.4s, v29.4s
	bne	1f
1:      uaba	v9.4s, v30.4s, v31.4s
        ldr	w0, [x4, x1, lsl #2]
        uaba	v10.4s, v28.4s, v29.4s
        bne	1f
1:      uaba	v11.4s, v30.4s, v31.4s
        ldr	w0, [x4, x2, lsl #2]
    .endr
	/*
	 * Check if timeout via global counter
	 */
	mrs	x11, cntvct_el0 // x11 stores the current cycle
	cmp	x10, x11
	b.gt	0b
	//b	0b

	mov	w0, #0
	ldp	d14, d15, [sp], #16
	ldp	d12, d13, [sp], #16
	ldp	d10, d11, [sp], #16
	ldp	d8, d9, [sp], #16
	stp	x11, x12, [sp], #16
	stp	x9, x10, [sp], #16
	ldp	x29, x30, [sp], #16
	ret

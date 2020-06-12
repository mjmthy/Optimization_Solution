#include <asm/vfpmacros.h>

        .text
        .arch armv7-a
        .global vfp_restore_state
        .type vfp_restore_state, %function
        .align 2
        .arm

vfp_restore_state:
	push        {r4-r12, lr}
	VFPFLDMIA   r0, r4
	ldmia       r0, {r5, r6, r7, r8}
#ifndef CONFIG_CPU_FEROCEON
	tst         r5, #FPEXC_EX
	beq         1f
	VFPFMXR     FPINST, r7
	tst         r5, #FPEXC_FP2V
	beq         1f
	VFPFMXR     FPINST2, r8
1:
#endif
	VFPFMXR     FPSCR, r6
	VFPFMXR     FPEXC, r5
	pop         {r4-r12, pc}


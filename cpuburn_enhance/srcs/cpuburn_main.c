/*
 * drivers/amlogic/cpuburn/cpuburn_main.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/ctype.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <asm/arch_timer.h>
#include <asm/neon.h>
#include <asm/barrier.h>
#ifndef CONFIG_ARM64
#include <asm/vfp.h>
#endif
#include "cpuburn.h"

#define CREATE_TRACE_POINTS
#include "cpuburn_trace.h"

struct cpuburn_dev *gdev;
int cpuid[CONFIG_NR_CPUS];
static int stop_hrtimer;

/*
 * by default neon codes are forbidded to execute
 * within interrupt context in arm, which is contrast
 * to arm64
 */
#ifndef CONFIG_ARM64
static DEFINE_PER_CPU(union vfp_state, hardirq_fpsimdstate);
static DEFINE_PER_CPU(union vfp_state, softirq_fpsimdstate);

#define vfpreg(_vfp_) #_vfp_

#define fmrx(_vfp_) ({			\
	u32 __v;			\
	asm("mrc p10, 7, %0, " vfpreg(_vfp_) \
	    ", cr0, 0 @ fmrx	%0, " #_vfp_	\
	    : "=r" (__v) : : "cc");	\
	__v;				\
})

#define fmxr(_vfp_, _var_)		\
	asm("mcr p10, 7, %0, " vfpreg(_vfp_) \
	    ", cr0, 0 @ fmxr	" #_vfp_ ", %0"	\
	   : : "r" (_var_) : "cc")

static void kernel_neon_isr_arm_begin(void)
{
	u32 fpexc;
	union vfp_state *s = this_cpu_ptr(in_irq() ?
					  &hardirq_fpsimdstate :
					  &softirq_fpsimdstate);

	fpexc = fmrx(FPEXC) | FPEXC_EN;
	fmxr(FPEXC, fpexc);

	vfp_save_state(s, fpexc);
}

static void kernel_neon_isr_arm_end(void)
{
	union vfp_state *s = this_cpu_ptr(in_irq() ?
					  &hardirq_fpsimdstate :
					  &softirq_fpsimdstate);

	vfp_restore_state(s);
	fmxr(FPEXC, fmrx(FPEXC) & ~FPEXC_EN);
}
#endif

static int cpuburn_thread(void *data)
{
	u64 start;
	u64 end;

	while (!kthread_should_stop()) {
		kernel_neon_begin();
		start = arch_counter_get_cntvct();
		routine(gdev->arch_timer_rate * USEC_PER_SEC * 5);
		end = arch_counter_get_cntvct();
		kernel_neon_end();
		//pr_info("duration: %llu cycles\n", end-start);
	}

	return 0;
}

enum hrtimer_restart cpuburn_isr(struct hrtimer *hrtimer)
{
	u64 start;
	u64 end;

#ifdef CONFIG_ARM64
	kernel_neon_begin();
#else
	kernel_neon_isr_arm_begin();
#endif
	start = arch_counter_get_cntvct();
	routine(gdev->on_cyc);
	end = arch_counter_get_cntvct();
#ifdef CONFIG_ARM64
	kernel_neon_end();
#else
	kernel_neon_isr_arm_end();
#endif
	//pr_info("duration: %llu cycles\n", end-start);
	trace_periodic_burn(start, end, end-start);

	if (!stop_hrtimer) {
		hrtimer_forward_now(hrtimer,
				    ns_to_ktime(gdev->off * NSEC_PER_USEC));
		return HRTIMER_RESTART;
	} else {
		return HRTIMER_NORESTART;
	}
}

static int cpuburn_prepare(void *data)
{
	int i = *(int *)data;

	//pr_info("cpuburn_prepare: %d\n", i);
	hrtimer_start(&gdev->cpuburn_hrtimer[i], gdev->trigger_time,
		      HRTIMER_MODE_ABS_PINNED);

	return 0;
}

static int burn_cpu_periodic(u32 cpun)
{
	int i;
	ktime_t kt;

	kt = ktime_set(2, 0);
	kt = ktime_add(kt, gdev->cpuburn_hrtimer[0].base->get_time());
	gdev->trigger_time = kt;

	/*
	 * Executing hrtimer_start on each CPU, so that we can make sure
	 * each hrimer with PINNED attribute have their isrs running on
	 * the targeted CPU
	 */
	for (i = 0; i < cpun; i++) {
		cpuid[i] = i;
		gdev->cpuburn_helper[i] = kthread_create(cpuburn_prepare,
							 &cpuid[i],
							 "cpuburn_helper");
		set_cpus_allowed_ptr(gdev->cpuburn_helper[i], cpumask_of(i));
		wake_up_process(gdev->cpuburn_helper[i]);
	}

	return 0;
}

static int burn_cpu_always(u32 cpun)
{
	int i;
	char name[16];

	for (i = 0; i < cpun; i++) {
		memset(name, 0x00, sizeof(name));
		sprintf(name, "cpuburn_%d", i);
		gdev->cpuburn_main[i] = kthread_create(cpuburn_thread,
						       NULL, name);
		if (IS_ERR(gdev->cpuburn_main[i])) {
			pr_err("Fail to create thread cpuburn_main[%d]\n", i);
			return PTR_ERR(gdev->cpuburn_main[i]);
		}
		set_cpus_allowed_ptr(gdev->cpuburn_main[i], cpumask_of(i));
		wake_up_process(gdev->cpuburn_main[i]);
	}

	return 0;
}

int burn_cpu(struct cpuburn_dev *dev)
{
	int ret;

	if (dev->cpun < 1 || dev->cpun > num_online_cpus())
		return -EINVAL;

	gdev = dev;

	switch (dev->mode) {
	case CPUBURN_MODE_ALWAYS:
		return burn_cpu_always(dev->cpun);
	break;
	case CPUBURN_MODE_PERIODIC:
		if (dev->on == 0 || dev->off == 0) {
			pr_err("please set none zero on/off for periodic mode\n");
			return -EINVAL;
		}
		ret = burn_cpu_periodic(dev->cpun);
		if (ret == 0)
			stop_hrtimer = 0;
		return ret;
	break;
	default:
		return -EINVAL;
	}
}

static int burn_stop_periodic(u32 cpun)
{
	stop_hrtimer = 1;

	return 0;
}

static int burn_stop_always(u32 cpun)
{
	int i;

	for (i = 0; i < cpun; i++)
		kthread_stop(gdev->cpuburn_main[i]);

	return 0;
}

int burn_stop(struct cpuburn_dev *dev)
{
	gdev = dev;

	switch (dev->mode) {
	case CPUBURN_MODE_ALWAYS:
		return burn_stop_always(dev->cpun);
	break;
	case CPUBURN_MODE_PERIODIC:
		return burn_stop_periodic(dev->cpun);
	break;
	default:
		return -EINVAL;
	}
}

#ifndef _CPUBURN_H_
#define _CPUBUTN_H_

#include <linux/cdev.h>

extern int rcu_cpu_stall_suppress;
extern int isr_check_en;
extern unsigned long watchdog_enabled;

/*
 * According to results of experiment, setting
 * on/off above 100us can achieve best results
 */
#define MIN_CPUBURN_ONOFF 100

enum cpuburn_mode {
	CPUBURN_MODE_ALWAYS,
	CPUBURN_MODE_PERIODIC,
	CPUBURN_MODE_INVALID,
};

enum cpuburn_state {
	CPUBURN_STATE_STOP,
	CPUBURN_STATE_RUN,
	CPUBURN_STATE_INVALID,
};

struct cpuburn_dev {
	struct class cls;
	int mode;
	u32 arch_timer_rate;
	unsigned long on;
	unsigned long off;
	u64 on_cyc;
	u64 off_cyc;
	struct hrtimer cpuburn_hrtimer[CONFIG_NR_CPUS];
	struct task_struct *cpuburn_helper[CONFIG_NR_CPUS];
	struct task_struct *cpuburn_main[CONFIG_NR_CPUS];
	ktime_t trigger_time;
	u32 cpun;
	int state;
};

unsigned long long routine(u64 cycle);
enum hrtimer_restart cpuburn_isr(struct hrtimer *hrtimer);
int burn_cpu(struct cpuburn_dev *dev);
int burn_stop(struct cpuburn_dev *dev);

extern void vfp_save_state(void *location, u32 fpexc);
extern void vfp_restore_state(void *location);

#endif

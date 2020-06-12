/*
 * drivers/amlogic/cpuburn/cpuburn_sysfs.c
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
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/ctype.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <asm/arch_timer.h>
#include <asm/barrier.h>
#include "cpuburn.h"

static ssize_t toggle_store(struct class *cla,
			    struct class_attribute *attr,
			    const char *buf,
			    size_t count)
{
	struct cpuburn_dev *dev;
	int ret;
	char str_op[3];
	int start = 0;

	ret = sscanf(buf, "%3s", str_op);
	if (ret != 1)
		return -EINVAL;

	dev = container_of(cla, struct cpuburn_dev, cls);
	if (strncmp(str_op, "off", 3) == 0) {
		if (dev->state != CPUBURN_STATE_RUN)
			return -EINVAL;
		start = 0;
	} else if (strncmp(str_op, "on", 2) == 0) {
		if (dev->state == CPUBURN_STATE_RUN)
			return -EINVAL;
		rcu_cpu_stall_suppress = 1;
		watchdog_enabled = 0;
		isr_check_en = 0;
		start = 1;
	} else {
		return -EINVAL;
	}

	if (start) {
		ret = burn_cpu(dev);
		if (ret != 0)
			return ret;
		dev->state = CPUBURN_STATE_RUN;
	} else {
		ret = burn_stop(dev);
		if (ret != 0)
			return ret;
		dev->state = CPUBURN_STATE_STOP;
	}

	return count;
}

static ssize_t dutyon_show(struct class *cla,
			   struct class_attribute *attr,
			   char *buf)

{
	struct cpuburn_dev *dev;
	ssize_t n = 0;

	dev = container_of(cla, struct cpuburn_dev, cls);
	n += sprintf(&buf[n], "%luus, %llucycles\n", dev->on, dev->on_cyc);
	buf[n] = 0;

	return n;
}

static ssize_t dutyon_store(struct class *cla,
			    struct class_attribute *attr,
			    const char *buf,
			    size_t count)
{
	int ret;
	struct cpuburn_dev *dev;

	dev = container_of(cla, struct cpuburn_dev, cls);
	ret = kstrtoul(buf, 10, &dev->on);
	if (ret != 0)
		return ret;

	if (dev->on < MIN_CPUBURN_ONOFF) {
		pr_err("please set value above 100us\n");
		return -EINVAL;
	}
	dev->on_cyc = dev->on * dev->arch_timer_rate;

	pr_info("set duty_on to: %luus, %llucycles\n", dev->on, dev->on_cyc);
	return count;
}

static ssize_t dutyoff_show(struct class *cla,
			    struct class_attribute *attr,
			    char *buf)
{
	struct cpuburn_dev *dev;
	ssize_t n = 0;

	dev = container_of(cla, struct cpuburn_dev, cls);
	n += sprintf(&buf[n], "%luus, %llucycles\n", dev->off, dev->off_cyc);
	buf[n] = 0;

	return n;
}

static ssize_t dutyoff_store(struct class *cla,
			     struct class_attribute *attr,
			     const char *buf,
			     size_t count)
{
	int ret;
	struct cpuburn_dev *dev;

	dev = container_of(cla, struct cpuburn_dev, cls);
	ret = kstrtoul(buf, 10, &dev->off);
	if (ret != 0)
		return ret;

	if (dev->on < MIN_CPUBURN_ONOFF) {
		pr_err("please set value above 100us\n");
		return -EINVAL;
	}
	dev->off_cyc = dev->off * dev->arch_timer_rate;

	pr_info("set duty_off to: %luus, %llucycles\n", dev->off, dev->off_cyc);
	return count;
}

static ssize_t available_modes_show(struct class *cla,
				    struct class_attribute *attr,
				    char *buf)
{
	struct cpuburn_dev *dev;
	ssize_t n = 0;

	dev = container_of(cla, struct cpuburn_dev, cls);
	n += sprintf(&buf[n], "always periodic");
	buf[n] = 0;

	return n;
}

static ssize_t mode_show(struct class *cla,
			 struct class_attribute *attr,
			 char *buf)
{
	struct cpuburn_dev *dev;
	ssize_t n = 0;
	static const char * const state[] = {"always", "periodic"};

	dev = container_of(cla, struct cpuburn_dev, cls);
	if (dev->mode != CPUBURN_MODE_ALWAYS &&
	    dev->mode != CPUBURN_MODE_PERIODIC)
		n += sprintf(&buf[n], "invalid");
	else
		n += sprintf(&buf[n], "%s", state[dev->mode]);
	buf[n] = 0;

	return n;
}

static ssize_t mode_store(struct class *cla,
			  struct class_attribute *attr,
			  const char *buf,
			  size_t count)
{
	int ret;
	struct cpuburn_dev *dev;
	char str_mode[8];

	ret = sscanf(buf, "%8s", str_mode);
	if (ret != 1)
		return -EINVAL;

	dev = container_of(cla, struct cpuburn_dev, cls);
	if (strncmp(str_mode, "always", 6) == 0)
		dev->mode = CPUBURN_MODE_ALWAYS;
	else if (strncmp(str_mode, "periodic", 8) == 0)
		dev->mode = CPUBURN_MODE_PERIODIC;
	else
		return -EINVAL;

	return count;
}

static ssize_t cpun_show(struct class *cla,
			 struct class_attribute *attr,
			 char *buf)
{
	struct cpuburn_dev *dev;
	ssize_t n = 0;

	dev = container_of(cla, struct cpuburn_dev, cls);
	n += sprintf(&buf[n], "%u", dev->cpun);
	buf[n] = 0;

	return n;
}

static ssize_t cpun_store(struct class *cla,
			  struct class_attribute *attr,
			  const char *buf,
			  size_t count)
{
	int ret;
	u32 cpun;
	struct cpuburn_dev *dev;

	ret = kstrtouint(buf, 10, &cpun);
	if (ret != 0)
		return -EINVAL;

	if (cpun > num_online_cpus() ||
	    cpun == 0)
		return -EINVAL;

	dev = container_of(cla, struct cpuburn_dev, cls);
	dev->cpun = cpun;

	return count;
}

static struct class_attribute cpuburn_class_attrs[] = {
	__ATTR_WO(toggle),
	__ATTR_RW(dutyon),
	__ATTR_RW(dutyoff),
	__ATTR_RO(available_modes),
	__ATTR_RW(mode),
	__ATTR_RW(cpun),
	__ATTR_NULL
};

static __init int cpuburn_probe(struct platform_device *pdev)
{
	struct cpuburn_dev *dev;
	int err;
	int i;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->cls.name = "cpuburn";
	dev->cls.owner = THIS_MODULE;
	dev->cls.class_attrs = cpuburn_class_attrs;
	err = class_register(&dev->cls);
	if (err)
		goto out_err0;

	/*
	 * we need arch_timer_rate to do us to cycle conversion
	 */
	dev->arch_timer_rate = arch_timer_get_cntfrq() / USEC_PER_SEC;
	pr_info("arch_timer_rate: %uMHZ\n", dev->arch_timer_rate);

	dev->mode = CPUBURN_MODE_INVALID;
	dev->cpun = num_online_cpus();
	dev->state = CPUBURN_STATE_STOP;

	for (i = 0; i < num_present_cpus(); i++) {
		hrtimer_init(&dev->cpuburn_hrtimer[i], CLOCK_MONOTONIC,
			     HRTIMER_MODE_REL);
		dev->cpuburn_hrtimer[i].function = cpuburn_isr;
	}

	return 0;

out_err0:
	devm_kfree(&pdev->dev, dev);
	return err;
}

static const struct of_device_id cpuburn_dt_match[] = {
	{	.compatible = "amlogic, cpuburn",
	},
	{},
};

static struct platform_driver cpuburn_platform_driver = {
	//.remove = cpuburn_remove,
	.driver = {
		.name = "cpuburn",
		.owner = THIS_MODULE,
		.of_match_table = cpuburn_dt_match,
	},
};

static __init int cpuburn_init(void)
{
	return platform_driver_probe(&cpuburn_platform_driver,
				     cpuburn_probe);
}

static __exit void cpuburn_exit(void)
{
	platform_driver_unregister(&cpuburn_platform_driver);
}

module_init(cpuburn_init);
module_exit(cpuburn_exit);

MODULE_DESCRIPTION("cpuburn kernel mode");
MODULE_LICENSE("GPL v2");

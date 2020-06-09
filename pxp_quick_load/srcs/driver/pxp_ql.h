/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __PXP_QL_H__
#define __PXP_QL_H__

#include <linux/cdev.h>

#define PXP_QL_GETINFO _IO('f', 0x70)
#define PXP_QL_REINIT  _IO('f', 0x71)
#define PXP_QL_RELEASE _IO('f', 0x72)
#define PXP_QL_SETSIZE _IO('f', 0x73)

struct pxp_ql_info {
	int size;
};

struct pxp_ql_dev {
	struct platform_device *pdev;
	struct class cls;
	struct cdev cdev;
	dev_t devno;
	struct page *pages;
	void *address;
	unsigned long size; /* specified by user */
	unsigned long pool_size;
};

#endif

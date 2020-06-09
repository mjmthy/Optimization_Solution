#include <linux/module.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/dma-contiguous.h>
#include <linux/uaccess.h>
#include <linux/cma.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include "pxp_ql.h"

#define PXP_QL_DEVICE_NAME "pxp_ql"
#define PXP_QL_CLASS_NAME  "pxp_ql"

static int pxp_ql_open(struct inode *inode, struct file *file)
{
	struct pxp_ql_dev *pxp_ql_dev;

	pxp_ql_dev = container_of(inode->i_cdev, struct pxp_ql_dev, cdev);
	file->private_data = pxp_ql_dev;

	return 0;
}

static int pxp_ql_memmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long vmsize;
	unsigned long pfn;
	struct device *dev;
	struct pxp_ql_dev *pxp_ql_dev;
	int ret;

	pxp_ql_dev = file->private_data;
	dev = &pxp_ql_dev->pdev->dev;

	if (!pxp_ql_dev->address) {
		dev_err(dev, "CMA pool not ready\n");
		return -ENOMEM;
	}

	vmsize = vma->vm_end - vma->vm_start;

	if (vmsize > pxp_ql_dev->pool_size) {
		dev_err(dev, "vma size too large\n");
		return -ENOMEM;
	}

	pfn = page_to_pfn(pxp_ql_dev->pages);
	ret = remap_pfn_range(vma, vma->vm_start, pfn,
			      vmsize, vma->vm_page_prot);
	if (ret) {
		dev_err(dev, "fail to remap\n");
		return ret;
	}

	/*
	 * incase we do the memmap op right after data being
	 * updated out of band
	 */
	if (pxp_ql_dev->address)
		__flush_dcache_area(pxp_ql_dev->address,
				    pxp_ql_dev->pool_size);

	return 0;
}

static int elf_header_check(Elf_Ehdr *hdr)
{
	if (memcmp(hdr->e_ident, ELFMAG, SELFMAG) != 0 ||
	    hdr->e_type != ET_REL ||
	    !elf_check_arch(hdr) ||
	    hdr->e_shentsize != sizeof(Elf_Shdr)) {
		pr_info("not a valid ko module\n");
		return -ENOEXEC;
	}

	return 0;
}

static int elf_module_size(Elf_Ehdr *hdr)
{
	if (elf_header_check(hdr))
		return 0;

	/*
	 * ko module does not have PHT &&
	 * have SHT at the end of elf file
	 */
	return hdr->e_shoff + hdr->e_shentsize * hdr->e_shnum;
}

static long pxp_ql_unlocked_ioctl(struct file *file,
				  unsigned int cmd,
				  unsigned long arg)
{
	struct pxp_ql_dev *pxp_ql_dev;
	struct pxp_ql_info *info;
	struct device *dev;
	void __user *argp = (void __user *)arg;
	int ret;

	pxp_ql_dev = file->private_data;
	dev = &pxp_ql_dev->pdev->dev;

	switch (cmd) {
	case PXP_QL_GETINFO:
		info = kmalloc(sizeof(*info), GFP_KERNEL);
		if (!info)
			return -ENOMEM;
		/*
		 * incase we do the getinfo op right after data being
		 * updated out of band
		 */
		if (pxp_ql_dev->address)
			__flush_dcache_area(pxp_ql_dev->address,
					    pxp_ql_dev->pool_size);

		/*
		 * If user has given the exact size of data to be restored(for
		 * the none ko case), just use this value as actual size.
		 */
		if (pxp_ql_dev->size)
			info->size = pxp_ql_dev->size;
		else
			info->size = elf_module_size(
					(Elf_Ehdr *)(pxp_ql_dev->address));
		ret = copy_to_user(argp, info, sizeof(*info));
		if (ret != 0) {
			dev_err(dev, "copy_to_user fail\n");
			kfree(info);
			return ret;
		}
		kfree(info);
		break;
	case PXP_QL_SETSIZE:
		info = kmalloc(sizeof(*info), GFP_KERNEL);
		if (!info)
			return -ENOMEM;

		ret = copy_from_user(info, argp, sizeof(*info));
		if (ret != 0) {
			dev_err(dev, "copy_from_user fail\n");
			kfree(info);
			return ret;
		}

		if (info->size > pxp_ql_dev->pool_size) {
			dev_err(dev, "size(%d) exceeds PXP_QUICK_LOAD CMA pool size(%lu)\n",
				info->size, pxp_ql_dev->pool_size);
			kfree(info);
			return -EINVAL;
		}
		pxp_ql_dev->size = info->size;
		dev_info(dev, "update size of data to be restored to %luBytes\n",
			 pxp_ql_dev->size);
		kfree(info);
		break;
	case PXP_QL_REINIT:
		if (pxp_ql_dev->pages) {
			dev_info(dev, "CMA pool was ready, no need to do reinit\n");
			return -EBUSY;
		}
		dev_info(dev, "Reinit CMA pool\n");
		pxp_ql_dev->pages = dma_alloc_from_contiguous(
				dev,
				pxp_ql_dev->pool_size >> PAGE_SHIFT,
				0);
		if (!pxp_ql_dev->pages) {
			dev_err(dev, "fail to allocate cma page\n");
			return -ENOMEM;
		}
		pxp_ql_dev->address = page_address(pxp_ql_dev->pages);
		__flush_dcache_area(pxp_ql_dev->address,
				    pxp_ql_dev->pool_size);

		break;
	case PXP_QL_RELEASE:
		if (pxp_ql_dev->pages) {
			dev_info(dev, "release CMA pool\n");
			dma_release_from_contiguous(dev, pxp_ql_dev->pages,
						    pxp_ql_dev->pool_size >>
						    PAGE_SHIFT);
			pxp_ql_dev->pages = NULL;
			pxp_ql_dev->address = NULL;
			pxp_ql_dev->size = 0;
		} else {
			dev_info(dev, "CMA pool was released, no need release again\n");
		}
		break;
	default:
		dev_err(dev, "unsupported ioctl cmd: 0x%x\n", cmd);
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_COMPAT
static long pxp_ql_compat_ioctl(struct file *file,
				unsigned int cmd,
				unsigned long arg)
{
	return pxp_ql_unlocked_ioctl(file, cmd,
				     (unsigned long)compat_ptr(arg));
}
#endif

static const struct file_operations pxp_ql_fops = {
	.owner = THIS_MODULE,
	.open = pxp_ql_open,
	.mmap = pxp_ql_memmap,
	.unlocked_ioctl = pxp_ql_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = pxp_ql_compat_ioctl,
#endif
};

static int __init aml_pxp_ql_probe(struct platform_device *pdev)
{
	int ret;
	struct device *devp;
	struct pxp_ql_dev *pxp_ql_dev;

	pxp_ql_dev = devm_kzalloc(&pdev->dev, sizeof(*pxp_ql_dev), GFP_KERNEL);
	if (unlikely(!pxp_ql_dev)) {
		dev_err(&pdev->dev, "fail to alloc mem\n");
		return -ENOMEM;
	}

	ret = alloc_chrdev_region(&pxp_ql_dev->devno, 0, 1,
				  PXP_QL_DEVICE_NAME);
	if (ret < 0) {
		dev_err(&pdev->dev, "fail to alloc major number\n");
		goto error_1;
	}
	pxp_ql_dev->cls.name = PXP_QL_CLASS_NAME;
	pxp_ql_dev->cls.owner = THIS_MODULE;
	ret = class_register(&pxp_ql_dev->cls);
	if (ret != 0) {
		dev_err(&pdev->dev, "fail to register class\n");
		goto error_2;
	}

	cdev_init(&pxp_ql_dev->cdev, &pxp_ql_fops);
	pxp_ql_dev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&pxp_ql_dev->cdev, pxp_ql_dev->devno, 1);
	if (ret != 0) {
		dev_err(&pdev->dev, "fail to add device\n");
		goto error_3;
	}
	devp = device_create(&pxp_ql_dev->cls, NULL, pxp_ql_dev->devno,
			     NULL, PXP_QL_DEVICE_NAME);
	if (IS_ERR(devp)) {
		dev_err(&pdev->dev, "fail to create device node\n");
		ret = PTR_ERR(devp);
		goto error_4;
	}

	pxp_ql_dev->pdev = pdev;
	platform_set_drvdata(pdev, pxp_ql_dev);

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "Couldn't claim our memory region\n");
		goto error_5;
	}

	pxp_ql_dev->pool_size = cma_get_size(pdev->dev.cma_area);
	pxp_ql_dev->pages = dma_alloc_from_contiguous(
				&pdev->dev,
				pxp_ql_dev->pool_size >> PAGE_SHIFT,
				0);
	if (!pxp_ql_dev->pages) {
		dev_err(&pdev->dev, "fail to allocate cma page\n");
		goto error_5;
	}
	pxp_ql_dev->address = page_address(pxp_ql_dev->pages);

	return 0;

error_5:
	device_destroy(&pxp_ql_dev->cls, pxp_ql_dev->devno);
error_4:
	cdev_del(&pxp_ql_dev->cdev);
error_3:
	class_unregister(&pxp_ql_dev->cls);
error_2:
	unregister_chrdev_region(pxp_ql_dev->devno, 1);
error_1:
	devm_kfree(&pdev->dev, pxp_ql_dev);
	return ret;
}

static int aml_pxp_ql_remove(struct platform_device *pdev)
{
	struct pxp_ql_dev *pxp_ql_dev;

	pxp_ql_dev = platform_get_drvdata(pdev);

	device_destroy(&pxp_ql_dev->cls, pxp_ql_dev->devno);
	cdev_del(&pxp_ql_dev->cdev);
	class_unregister(&pxp_ql_dev->cls);
	unregister_chrdev_region(pxp_ql_dev->devno, 1);
	of_reserved_mem_device_release(&pdev->dev);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id pxp_ql_dt_match[] = {
	{
		.compatible = "xxx,pxp_ql",
	},
	{},
};

static struct platform_driver pxp_ql_platform_driver = {
	.remove = aml_pxp_ql_remove,
	.driver = {
		.name = "pxp_ql",
		.owner = THIS_MODULE,
		.of_match_table = pxp_ql_dt_match,
	},
};

static int __init aml_pxp_ql_init(void)
{
	return platform_driver_probe(&pxp_ql_platform_driver,
				     aml_pxp_ql_probe);
}

static void __exit aml_pxp_ql_exit(void)
{
	platform_driver_unregister(&pxp_ql_platform_driver);
}

module_init(aml_pxp_ql_init)
module_exit(aml_pxp_ql_exit)

MODULE_DESCRIPTION("Amlogic PXP Quick Load driver");
MODULE_LICENSE("GPL");

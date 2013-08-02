/*
 *  Backlight Driver for Openframe devices
 *
 *  Copyright (c) 2010 Andrew de Quincey
 *
 *  Based onprogear_bl.c driver by Marcin Juszkiewicz
 *  <linux at hrw dot one dot pl>
 *
 *  Based on Progear LCD driver by M Schacht
 *  <mschacht at alumni dot washington dot edu>
 *
 *  Based on Sharp's Corgi Backlight Driver
 *  Based on Backlight Driver for HP Jornada 680
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/pci.h>

static struct pci_dev *gfx_dev = NULL;
static u32 *bl_baseptr = NULL;

static int openframe_set_intensity(struct backlight_device *bd)
{
	int intensity = bd->props.brightness;

	if (bd->props.power != FB_BLANK_UNBLANK)
		intensity = 0;
	if (bd->props.fb_blank != FB_BLANK_UNBLANK)
		intensity = 0;

	*bl_baseptr = (intensity << 1) | 0x400000;

	return 0;
}

static int openframe_get_intensity(struct backlight_device *bd)
{
	int intensity;

	intensity = (*bl_baseptr >> 1) & 0x2f;

	return intensity;
}

static struct backlight_ops openframe_ops = {
	.get_brightness = openframe_get_intensity,
	.update_status = openframe_set_intensity,
};

static int openframe_probe(struct platform_device *pdev)
{
	u32 temp;
	struct backlight_device *openframe_backlight_device;
	struct backlight_properties props;

	gfx_dev = pci_get_device(PCI_VENDOR_ID_INTEL, 0x8108, NULL);
	if (!gfx_dev) {
		printk("Intel SCH Poulsbo graphics controller not found.\n");
		return -ENODEV;
	}

	pci_read_config_dword(gfx_dev, 16, &temp);
	bl_baseptr = ioremap(temp + 0x61254, 4);
	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = 32;
	props.power = FB_BLANK_UNBLANK;
	props.type = BACKLIGHT_PLATFORM;
	props.brightness = 32;
	openframe_backlight_device = backlight_device_register("openframe-bl",
							     &pdev->dev, NULL,
							     &openframe_ops, &props);
	if (IS_ERR(openframe_backlight_device)) {
		iounmap(bl_baseptr);
		pci_dev_put(gfx_dev);
		return PTR_ERR(openframe_backlight_device);
	}
	platform_set_drvdata(pdev, openframe_backlight_device);

	openframe_set_intensity(openframe_backlight_device);

	return 0;
}

static int openframe_remove(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);
	backlight_device_unregister(bd);

	iounmap(bl_baseptr);
	pci_dev_put(gfx_dev);

	return 0;
}

static struct platform_driver openframe_driver = {
	.probe = openframe_probe,
	.remove = openframe_remove,
	.driver = {
		   .name = "openframe-bl",
		   },
};

static struct platform_device *openframe_device;

static int __init openframe_init(void)
{
	int ret = platform_driver_register(&openframe_driver);

	if (ret)
		return ret;
	openframe_device = platform_device_register_simple("openframe-bl", -1,
								NULL, 0);
	if (IS_ERR(openframe_device)) {
		platform_driver_unregister(&openframe_driver);
		return PTR_ERR(openframe_device);
	}

	return 0;
}

static void __exit openframe_exit(void)
{
	platform_device_unregister(openframe_device);
	platform_driver_unregister(&openframe_driver);
}

module_init(openframe_init);
module_exit(openframe_exit);

MODULE_AUTHOR("Andrew de Quincey <adq@lidskialf.net>");
MODULE_DESCRIPTION("Openframe Backlight Driver");
MODULE_LICENSE("GPL");

// SPDX-License-Identifier: GPL-2.0
/*
 * AD7606 platform driver for RP1 PIO SPI (no hardware spi0).
 */

#include <linux/iio/buffer-dmaengine.h>
#include <linux/iio/iio.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include "ad7606.h"
#include "ad7606_rp1_pio.h"

static int ad7606_rp1_read_block(struct device *dev, int count, void *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct ad7606_state *st = iio_priv(indio_dev);

	if (!st->rp1_pio_en)
		return -ENODEV;

	return ad7606_rp1_pio_read_block(st->bus_data, count, buf);
}

static const struct ad7606_bus_ops ad7606_rp1_bops = {
	.offload_config = ad7606_rp1_pio_probe,
	.read_block = ad7606_rp1_read_block,
};

static int ad7606_rp1_platform_probe(struct platform_device *pdev)
{
	int irq = platform_get_irq(pdev, 0);

	return ad7606_probe(&pdev->dev, irq, NULL, &ad7606_8_info,
			    &ad7606_rp1_bops);
}

static void ad7606_rp1_platform_remove(struct platform_device *pdev)
{
	ad7606_rp1_pio_remove(&pdev->dev);
}

static const struct of_device_id ad7606_rp1_of_match[] = {
	{ .compatible = "ad7606,rp1-pio", },
	{ }
};
MODULE_DEVICE_TABLE(of, ad7606_rp1_of_match);

struct platform_driver ad7606_rp1_driver = {
	.probe = ad7606_rp1_platform_probe,
	.remove = ad7606_rp1_platform_remove,
	.driver = {
		.name = "ad7606-rp1",
		.of_match_table = ad7606_rp1_of_match,
		.pm = AD7606_PM_OPS,
	},
};


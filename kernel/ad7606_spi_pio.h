/* SPDX-License-Identifier: GPL-2.0 */
/*
 * AD7606 SPI read sequencer for RP1 PIO (mode 3).
 * Generated from ad7606_spi.pio via pioasm (make pio).
 *
 * CS assert [1] after BUSY low; bit loop keeps [1] delays (4 SM cycles/bit).
 * 525 PIO cycles/frame — do not remove bit delays (8 MHz SPI corrupts data).
 */

#ifndef AD7606_SPI_PIO_H
#define AD7606_SPI_PIO_H

#include <linux/pio_rp1.h>

#define ad7606_spi_read_wrap_target	0
#define ad7606_spi_read_wrap		8

static const uint16_t ad7606_spi_read_program_instructions[] = {
	0x2099, /* wait 1 gpio, 25 */
	0x2019, /* wait 0 gpio, 25 */
	0xf900, /* set pins, 0         side 1 [1] */
	0xf823, /* set x, 3            side 1 */
	0xf85f, /* set y, 31           side 1 */
	0x5901, /* in pins, 1          side 1 [1] */
	0x1185, /* jmp y--, 5          side 0 [1] */
	0x1844, /* jmp x--, 4          side 1 */
	0xf901, /* set pins, 1         side 1 [1] */
};

static const pio_program_t ad7606_spi_read_program = {
	.instructions = ad7606_spi_read_program_instructions,
	.length = ARRAY_SIZE(ad7606_spi_read_program_instructions),
	.origin = -1,
};

static inline pio_sm_config ad7606_spi_read_default_config(uint offset)
{
	pio_sm_config c = pio_get_default_sm_config();

	sm_config_set_wrap(&c, offset + ad7606_spi_read_wrap_target,
			   offset + ad7606_spi_read_wrap);
	sm_config_set_sideset(&c, 2, true, false);
	return c;
}

#endif /* AD7606_SPI_PIO_H */

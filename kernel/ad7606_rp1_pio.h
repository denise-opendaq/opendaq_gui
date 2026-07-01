/* SPDX-License-Identifier: GPL-2.0 */
#ifndef AD7606_RP1_PIO_H
#define AD7606_RP1_PIO_H

#include <linux/atomic.h>
#include <linux/dma-mapping.h>
#include <linux/types.h>

struct device;
struct iio_dev;
struct completion;

struct ad7606_rp1_pio_data {
	struct rp1_pio_client *pio;
	struct device *dev;
	struct device *pio_dma_dev; /* dmaengine channel device for coherent DMA */
	unsigned int sm;
	unsigned int offset;
	bool enabled;
	bool use_dma;

	/* First N channels on SPI (1..8); PIO may clock extra bits to 32b boundary. */
	unsigned int read_channels;
	unsigned int pio_words_per_frame;
	unsigned int frame_bytes;
	unsigned int pio_x;
	unsigned int pio_y;
	u16 insns[9];

	u32 *dma_buf;
	dma_addr_t dma_handle;
	unsigned int dma_buf_bytes;
	struct completion *dma_done;

	u16 *frame_buf;

	struct iio_dev *indio_dev;
	struct task_struct *capture_task;
	atomic_t capture_run;
	wait_queue_head_t idle_wait;
	wait_queue_head_t buffer_wait;
	bool capture_idle;
	bool hw_active;
	atomic64_t dropped_frames;
	unsigned int misalign_events;
	unsigned int dma_fail_count;
};

#define AD7606_RP1_PIN_CS	8
#define AD7606_RP1_PIN_MISO	9
#define AD7606_RP1_PIN_SCLK	11
#define AD7606_RP1_PIN_BUSY	25

#define AD7606_RP1_MAX_CHANNELS		8
#define AD7606_RP1_MAX_FRAME_BYTES	(AD7606_RP1_MAX_CHANNELS * sizeof(u32))
#define AD7606_RP1_BURST_MAX		128
#define AD7606_RP1_DMA_BUF_BYTES	4096
#define AD7606_RP1_DMA_XFER_BUFS	4

int ad7606_rp1_pio_probe(struct device *dev, struct iio_dev *indio_dev);
void ad7606_rp1_pio_remove(struct device *dev);
void ad7606_rp1_pio_shutdown(struct device *dev);
int ad7606_rp1_pio_read_block(struct ad7606_rp1_pio_data *pio_data,
			      int count, void *buf);
int ad7606_rp1_stream_session_arm(struct ad7606_rp1_pio_data *pd);
void ad7606_rp1_stream_session_disarm(struct ad7606_rp1_pio_data *pd);
void ad7606_rp1_reset_stream_counters(struct ad7606_rp1_pio_data *pd);
int ad7606_rp1_set_spi_channels(struct ad7606_rp1_pio_data *pd, unsigned int ch);
unsigned int ad7606_rp1_get_spi_channels(struct ad7606_rp1_pio_data *pd);
int ad7606_rp1_register_sysfs(struct iio_dev *indio_dev);

#endif /* AD7606_RP1_PIO_H */

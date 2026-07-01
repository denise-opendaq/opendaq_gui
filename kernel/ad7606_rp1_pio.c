// SPDX-License-Identifier: GPL-2.0
/*
 * AD7606 SPI capture via RP1 PIO (kernel rp1-pio driver).
 *
 * Max-rate path: one PIO DMA transfer per burst (up to 128 frames) after FIFO
 * frame-alignment.  Each pio_sm_get() is an RP1 firmware RPC — bulk DMA avoids
 * 8× RPC per frame.  DMA buffers are allocated on the rp1-pio dmaengine
 * channel device (not the PIO platform device — it has no DMA mapping).
 */

#include <linux/cleanup.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/iio/buffer.h>
#include <linux/iio/buffer_impl.h>
#include <linux/iio/iio.h>
#include <linux/iio/iio-opaque.h>
#include <linux/jiffies.h>
#include <linux/ktime.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pio_rp1.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/swab.h>
#include <linux/sysfs.h>

#include "ad7606.h"
#include "ad7606_rp1_pio.h"
#include "ad7606_spi_pio.h"

/* Default CLKDIV=6: ~16.7MHz SM, 4 cycles/bit -> ~4.2MHz SPI.
 * Frame = 525 PIO cycles -> ~31us SPI read; target ~29-30kHz stable. */
#define AD7606_RP1_PIO_CLKDIV_DEFAULT	6

/* RP1 PIO SM DMACTRL_RX register bits (proc_pio.h not exported to kernel) */
#define AD7606_RP1_DMACTRL_RX_DREQ_EN	BIT(31)
#define AD7606_RP1_DMACTRL_RX_DWELL(n)	((n) << 7)
#define AD7606_RP1_DMACTRL_RX_THRESH(n)	((n) << 0)

#define AD7606_RP1_DMA_FAIL_MAX		5
#define AD7606_RP1_BUFFER_WAIT_MS	10

static struct ad7606_rp1_pio_data *ad7606_rp1_active_pd;

static int ad7606_rp1_pio_sm_init(struct ad7606_rp1_pio_data *pd);
static int ad7606_rp1_pio_program_init(struct ad7606_rp1_pio_data *pd);
static int ad7606_rp1_reinstall_program(struct ad7606_rp1_pio_data *pd);
static void ad7606_rp1_apply_channel_config(struct ad7606_rp1_pio_data *pd,
					    unsigned int ch);
static const pio_program_t *ad7606_rp1_pio_program(struct ad7606_rp1_pio_data *pd);

static int ad7606_rp1_reinstall_program(struct ad7606_rp1_pio_data *pd)
{
	const pio_program_t *prog;
	int ret;

	if (!pd->pio || pd->sm < 0)
		return -ENODEV;

	ret = pio_sm_set_enabled(pd->pio, pd->sm, false);
	if (ret)
		return ret;

	if (pd->offset != PIO_ORIGIN_INVALID) {
		prog = ad7606_rp1_pio_program(pd);
		pio_remove_program(pd->pio, prog, pd->offset);
		pd->offset = PIO_ORIGIN_INVALID;
	}

	prog = ad7606_rp1_pio_program(pd);
	if (!pio_can_add_program(pd->pio, prog))
		return -ENOMEM;

	pd->offset = pio_add_program(pd->pio, prog);
	if (pd->offset == PIO_ORIGIN_INVALID)
		return -EIO;

	ret = ad7606_rp1_pio_sm_init(pd);
	if (ret)
		return ret;

	return pio_sm_clear_fifos(pd->pio, pd->sm);
}

int ad7606_rp1_set_spi_channels(struct ad7606_rp1_pio_data *pd, unsigned int ch)
{
	struct ad7606_state *st;
	int ret;

	if (!pd || !pd->enabled)
		return -ENODEV;
	if (ch < 1 || ch > AD7606_RP1_MAX_CHANNELS)
		return -EINVAL;
	if (iio_buffer_enabled(pd->indio_dev))
		return -EBUSY;
	if (ad7606_iio_chrdev_is_open(pd->indio_dev))
		return -EBUSY;
	if (atomic_read(&pd->capture_run))
		return -EBUSY;
	if (ch == pd->read_channels)
		return 0;

	st = iio_priv(pd->indio_dev);
	guard(mutex)(&st->lock);

	ad7606_rp1_apply_channel_config(pd, ch);
	ret = ad7606_rp1_reinstall_program(pd);
	if (ret)
		return ret;

	dev_info(pd->dev, "PIO SPI channels: %u (%u FIFO words/frame)\n",
		 pd->read_channels, pd->pio_words_per_frame);
	return 0;
}
EXPORT_SYMBOL_GPL(ad7606_rp1_set_spi_channels);

unsigned int ad7606_rp1_get_spi_channels(struct ad7606_rp1_pio_data *pd)
{
	if (!pd)
		return AD7606_RP1_MAX_CHANNELS;

	return pd->read_channels;
}
EXPORT_SYMBOL_GPL(ad7606_rp1_get_spi_channels);

static ssize_t pio_spi_channels_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct ad7606_state *st = iio_priv(indio_dev);

	if (!st->rp1_pio_en)
		return -ENODEV;

	return sysfs_emit(buf, "%u\n",
			  ad7606_rp1_get_spi_channels(st->bus_data));
}

static ssize_t pio_spi_channels_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct ad7606_state *st = iio_priv(indio_dev);
	unsigned int ch;
	int ret;

	if (!st->rp1_pio_en)
		return -ENODEV;

	ret = kstrtouint(buf, 0, &ch);
	if (ret)
		return ret;

	ret = ad7606_rp1_set_spi_channels(st->bus_data, ch);
	if (ret)
		return ret;

	return len;
}

static DEVICE_ATTR_RW(pio_spi_channels);

static struct attribute *ad7606_rp1_sysfs_attrs[] = {
	&dev_attr_pio_spi_channels.attr,
	NULL,
};

static const struct attribute_group ad7606_rp1_sysfs_group = {
	.attrs = ad7606_rp1_sysfs_attrs,
};

static bool ad7606_rp1_use_dma = true;
static int ad7606_rp1_clkdiv = AD7606_RP1_PIO_CLKDIV_DEFAULT;
static int ad7606_rp1_channels = AD7606_RP1_MAX_CHANNELS;
module_param_named(clkdiv, ad7606_rp1_clkdiv, int, 0644);
MODULE_PARM_DESC(clkdiv, "PIO SM clock divider (default 6; lower=faster, min ~4)");
module_param_named(channels, ad7606_rp1_channels, int, 0644);
MODULE_PARM_DESC(channels,
		 "SPI read first N channels at boot (1-8); runtime: pio_spi_channels sysfs");
module_param_named(dma, ad7606_rp1_use_dma, bool, 0644);
MODULE_PARM_DESC(dma, "PIO burst DMA (default on; set 0 for FIFO-only)");

static unsigned int ad7606_rp1_pio_words(unsigned int ch)
{
	if (ch <= 1)
		return 1;

	return DIV_ROUND_UP(ch * 16, 32) * 2;
}

static void ad7606_rp1_pio_loop(unsigned int ch, unsigned int *x, unsigned int *y)
{
	unsigned int spi_bits;

	if (ch <= 1) {
		*x = 0;
		*y = 15;
		return;
	}

	spi_bits = DIV_ROUND_UP(ch * 16, 32) * 32;
	*x = spi_bits / 32 - 1;
	*y = 31;
}

static void ad7606_rp1_apply_channel_config(struct ad7606_rp1_pio_data *pd,
					    unsigned int ch)
{
	if (ch < 1)
		ch = 1;
	if (ch > AD7606_RP1_MAX_CHANNELS)
		ch = AD7606_RP1_MAX_CHANNELS;

	pd->read_channels = ch;
	pd->pio_words_per_frame = ad7606_rp1_pio_words(ch);
	pd->frame_bytes = pd->pio_words_per_frame * sizeof(u32);
	ad7606_rp1_pio_loop(ch, &pd->pio_x, &pd->pio_y);

	memcpy(pd->insns, ad7606_spi_read_program_instructions,
	       sizeof(pd->insns));
	pd->insns[3] = 0xf820 | (pd->pio_x & 0x1f);
	pd->insns[4] = 0xf840 | (pd->pio_y & 0x1f);
}

static const pio_program_t *ad7606_rp1_pio_program(struct ad7606_rp1_pio_data *pd)
{
	static pio_program_t prog;

	prog = ad7606_spi_read_program;
	prog.instructions = pd->insns;
	return &prog;
}

static void ad7606_rp1_dma_done(void *param)
{
	struct ad7606_rp1_pio_data *pd = param;

	if (pd->dma_done)
		complete(pd->dma_done);
}

static int ad7606_rp1_pio_match(struct device *dev, const void *data)
{
	return of_device_is_compatible(dev_of_node(dev), "raspberrypi,rp1-pio");
}

static struct device *ad7606_rp1_find_pio_dma_dev(void)
{
	return bus_find_device(&platform_bus_type, NULL, NULL,
			       ad7606_rp1_pio_match);
}

static bool ad7606_rp1_buffer_has_space(struct iio_dev *indio_dev)
{
	struct iio_buffer *buffer = indio_dev->buffer;

	if (!buffer || !buffer->access || !buffer->access->space_available)
		return true;

	/* space_available returns entry count (not bytes) in IIO kfifo */
	return buffer->access->space_available(buffer) >= 1;
}

static int ad7606_rp1_buffer_frames_available(struct iio_dev *indio_dev)
{
	struct iio_buffer *buffer = indio_dev->buffer;
	int space;

	if (!buffer || !buffer->access || !buffer->access->space_available)
		return AD7606_RP1_BURST_MAX;

	/* space_available returns entry count (not bytes) in IIO kfifo */
	space = buffer->access->space_available(buffer);
	if (space < 0)
		return 0;

	return min_t(int, space, AD7606_RP1_BURST_MAX);
}

static void ad7606_rp1_count_dropped(struct ad7606_rp1_pio_data *pd, u64 n)
{
	u64 total;

	if (!n)
		return;

	total = atomic64_add_return(n, &pd->dropped_frames);
	if (total == n)
		dev_warn_ratelimited(pd->dev,
				     "IIO buffer full, dropping capture frames\n");
}

/*
 * FIFO-depth backfill when BUSY IRQ ring is empty (startup / transient).
 */
static s64 ad7606_rp1_frame_timestamp(struct ad7606_state *st, s64 anchor_ns,
				      unsigned int newer_frames_in_fifo)
{
	if (!st->rp1_ts_period_ns || !newer_frames_in_fifo)
		return anchor_ns;

	return anchor_ns - (s64)newer_frames_in_fifo * st->rp1_ts_period_ns;
}

/*
 * Prefer BUSY-edge conversion time; fall back to iio_get_time_ns() with FIFO
 * depth backfill when the IRQ ring is momentarily empty.
 */
static s64 ad7606_rp1_sample_timestamp(struct iio_dev *indio_dev,
				       struct ad7606_state *st,
				       s64 anchor_ns,
				       unsigned int fifo_frames)
{
	s64 ts;

	if (!ad7606_busy_ts_pop_fifo(st, &ts, fifo_frames))
		return ts;

	return ad7606_rp1_frame_timestamp(st, anchor_ns,
					  fifo_frames ? fifo_frames - 1 : 0);
}

static int ad7606_rp1_push_frame(struct iio_dev *indio_dev,
				 struct ad7606_state *st, u16 *buf,
				 unsigned int read_channels, s64 ts)
{
	memset(buf + read_channels, 0,
	       (AD7606_RP1_MAX_CHANNELS - read_channels) * sizeof(*buf));

	if (!ACCESS_PRIVATE(indio_dev, scan_timestamp))
		return iio_push_to_buffers(indio_dev, buf);

	return iio_push_to_buffers_with_ts(indio_dev, buf, indio_dev->scan_bytes, ts);
}

static void ad7606_rp1_unpack_raw_frame(struct ad7606_rp1_pio_data *pd,
					const u32 *raw, u16 *out)
{
	int i;

	for (i = 0; i < pd->read_channels; i++)
		out[i] = swab16((u16)(raw[i] >> 16));
}

static int ad7606_rp1_fifo_rx_level(struct ad7606_rp1_pio_data *pd)
{
	return pio_sm_rx_fifo_level(pd->pio, pd->sm);
}

/* Whole frames only; leave a trailing partial frame in the FIFO (do not drop from head). */
static int ad7606_rp1_fifo_complete_frames(struct ad7606_rp1_pio_data *pd, int level)
{
	unsigned int wpf = pd->pio_words_per_frame;
	int rem;

	if (level <= 0 || !wpf)
		return 0;

	rem = level % wpf;
	if (rem)
		pd->misalign_events++;

	return level / wpf;
}

static int ad7606_rp1_wait_fifo_frames(struct ad7606_rp1_pio_data *pd,
				       int min_frames, unsigned long timeout_ms)
{
	unsigned long deadline = jiffies + msecs_to_jiffies(timeout_ms);
	unsigned int wpf = pd->pio_words_per_frame;
	int need = min_frames * wpf;
	int level, frames;

	do {
		level = ad7606_rp1_fifo_rx_level(pd);
		if (level < 0)
			return level;
		if (level >= need)
			break;
		cpu_relax();
	} while (time_before(jiffies, deadline));

	frames = ad7606_rp1_fifo_complete_frames(pd, level);
	if (frames < min_frames)
		return 0;

	return frames;
}

static int ad7606_rp1_dma_alloc_buf(struct ad7606_rp1_pio_data *pd)
{
	struct device *pio_dev;
	struct device *dma_dev;
	struct dma_chan *chan;
	char chan_name[8];

	if (pd->dma_buf)
		return 0;

	pio_dev = ad7606_rp1_find_pio_dma_dev();
	if (!pio_dev) {
		dev_warn(pd->dev, "rp1-pio device not found, DMA disabled\n");
		return -ENODEV;
	}

	snprintf(chan_name, sizeof(chan_name), "rx%u", pd->sm);
	chan = dma_request_chan(pio_dev, chan_name);
	put_device(pio_dev);
	if (IS_ERR(chan)) {
		dev_warn(pd->dev, "PIO DMA channel %s request failed (%ld)\n",
			 chan_name, PTR_ERR(chan));
		return PTR_ERR(chan);
	}

	dma_dev = chan->device->dev;
	get_device(dma_dev);

	pd->dma_buf_bytes = AD7606_RP1_DMA_BUF_BYTES;
	pd->dma_buf = dma_alloc_coherent(dma_dev, pd->dma_buf_bytes,
					 &pd->dma_handle, GFP_KERNEL);
	dma_release_channel(chan);

	if (!pd->dma_buf) {
		dev_warn(pd->dev, "PIO DMA buffer alloc failed on %s (%u bytes)\n",
			 dev_name(dma_dev), pd->dma_buf_bytes);
		put_device(dma_dev);
		return -ENOMEM;
	}

	pd->pio_dma_dev = dma_dev;
	return 0;
}

static int ad7606_rp1_read_frame_fifo(struct ad7606_rp1_pio_data *pd, u16 *out)
{
	unsigned int wpf = pd->pio_words_per_frame;
	int i;

	for (i = 0; i < wpf; i++) {
		u32 raw = pio_sm_get_blocking(pd->pio, pd->sm);

		if (i < pd->read_channels)
			out[i] = swab16((u16)(raw >> 16));
	}

	return pio_get_error(pd->pio);
}

/*
 * Burst DMA: rp1-pio with dma_addr=0 uses internal bounce buffers and does
 * NOT copy into our virt pointer on RX — pass pd->dma_handle so the RP1
 * DMA engine writes directly into our coherent buffer.
 *
 * Only transfer whole frames already present in the PIO RX FIFO (same
 * alignment rules as the FIFO poll path).
 */
static int ad7606_rp1_capture_burst_dma(struct ad7606_rp1_pio_data *pd,
					 struct iio_dev *indio_dev,
					 struct ad7606_state *st, int nframes)
{
	unsigned int bytes;
	struct completion done;
	int ret, i, fifo_frames, level;

	if (!pd->dma_buf || !pd->dma_handle)
		return -EINVAL;

	fifo_frames = ad7606_rp1_wait_fifo_frames(pd, 1, 50);
	if (fifo_frames <= 0)
		return fifo_frames;
	nframes = min(nframes, fifo_frames);

	/* Re-snapshot: transfer only whole frames already in the FIFO. */
	level = ad7606_rp1_fifo_rx_level(pd);
	if (level < 0)
		return level;
	fifo_frames = ad7606_rp1_fifo_complete_frames(pd, level);
	if (fifo_frames <= 0)
		return 0;
	nframes = min(nframes, fifo_frames);

	bytes = nframes * pd->frame_bytes;
	if (bytes > pd->dma_buf_bytes)
		return -EINVAL;

	init_completion(&done);
	pd->dma_done = &done;

	ret = pio_sm_xfer_data(pd->pio, pd->sm, PIO_DIR_FROM_SM,
			       bytes, pd->dma_buf, pd->dma_handle,
			       ad7606_rp1_dma_done, pd);
	if (ret) {
		pd->dma_done = NULL;
		return ret;
	}

	if (!wait_for_completion_timeout(&done,
					 msecs_to_jiffies(nframes * 2 + 200))) {
		pd->dma_done = NULL;
		return -ETIMEDOUT;
	}
	pd->dma_done = NULL;

	ret = pio_get_error(pd->pio);
	if (ret)
		return ret;

	{
		s64 anchor_ns = iio_get_time_ns(indio_dev);

		for (i = 0; i < nframes; i++) {
			u16 frame[AD7606_RP1_MAX_CHANNELS];
			s64 ts = ad7606_rp1_sample_timestamp(indio_dev, st, anchor_ns,
							     nframes - i);

			ad7606_rp1_unpack_raw_frame(pd,
				pd->dma_buf + i * pd->pio_words_per_frame, frame);

			if (!ad7606_rp1_buffer_has_space(indio_dev)) {
				ad7606_rp1_count_dropped(pd, nframes - i);
				return i ? i : -ENOSPC;
			}

			ret = ad7606_rp1_push_frame(indio_dev, st, frame,
						    pd->read_channels, ts);
			if (ret) {
				ad7606_rp1_count_dropped(pd, nframes - i);
				return ret;
			}
		}
	}

	return nframes;
}

/*
 * Capture up to max_frames.  In DMA mode: one bulk transfer drains the FIFO
 * for all frames at once.  Falls back to per-frame FIFO reads if DMA fails.
 */
static int ad7606_rp1_capture_burst(struct ad7606_rp1_pio_data *pd,
				    struct iio_dev *indio_dev,
				    struct ad7606_state *st, int max_frames)
{
	int nframes, frames = 0, level, fifo_frames, ret;

	if (pd->use_dma && max_frames > 0) {
		nframes = min_t(int, max_frames,
				(int)(pd->dma_buf_bytes / pd->frame_bytes));
		ret = ad7606_rp1_capture_burst_dma(pd, indio_dev, st, nframes);
		if (ret > 0) {
			pd->dma_fail_count = 0;
			return ret;
		}
		if (ret == 0)
			return 0;
		if (ret == -ENOSPC)
			return 0;

		pd->dma_fail_count++;
		if (pd->dma_fail_count >= AD7606_RP1_DMA_FAIL_MAX) {
			pd->use_dma = false;
			dev_warn_once(pd->dev,
				      "PIO burst DMA failed %u times, FIFO only\n",
				      pd->dma_fail_count);
		} else {
			dev_warn_ratelimited(pd->dev,
					     "PIO burst DMA failed (%d), retry FIFO\n",
					     ret);
		}
	}

	while (frames < max_frames) {
		if (!ad7606_rp1_buffer_has_space(indio_dev))
			break;

		level = ad7606_rp1_fifo_rx_level(pd);
		if (level < 0)
			return level;
		fifo_frames = ad7606_rp1_fifo_complete_frames(pd, level);
		if (fifo_frames < 1)
			break;

		{
			s64 anchor_ns = iio_get_time_ns(indio_dev);
			s64 ts = ad7606_rp1_sample_timestamp(indio_dev, st, anchor_ns,
							     fifo_frames);

			ret = ad7606_rp1_read_frame_fifo(pd, st->data.buf16);
			if (ret)
				return ret;

			ret = ad7606_rp1_push_frame(indio_dev, st, st->data.buf16,
						    pd->read_channels, ts);
		}
		if (ret) {
			ad7606_rp1_count_dropped(pd, 1);
			return ret;
		}

		frames++;
	}

	return frames;
}

static int ad7606_rp1_hw_start(struct ad7606_rp1_pio_data *pd)
{
	int ret;

	ret = pio_sm_set_enabled(pd->pio, pd->sm, false);
	if (ret)
		return ret;

	ret = pio_sm_clear_fifos(pd->pio, pd->sm);
	if (ret)
		return ret;

	/* Re-init SM to restore clkdiv and all config (restart resets clkdiv) */
	ret = ad7606_rp1_pio_program_init(pd);
	if (ret)
		return ret;

	msleep(20);

	return pio_sm_set_enabled(pd->pio, pd->sm, true);
}

static void ad7606_rp1_hw_stop(struct ad7606_rp1_pio_data *pd)
{
	pio_sm_clear_fifos(pd->pio, pd->sm);
}

static int ad7606_rp1_hw_capture_start(struct ad7606_rp1_pio_data *pd);
static void ad7606_rp1_hw_capture_stop(struct ad7606_rp1_pio_data *pd);
static int ad7606_rp1_pwm_stream_start(struct ad7606_rp1_pio_data *pd);
static void ad7606_rp1_pwm_stream_stop(struct ad7606_rp1_pio_data *pd);

static int ad7606_rp1_capture_thread(void *arg)
{
	struct ad7606_rp1_pio_data *pd = arg;
	struct iio_dev *indio_dev = pd->indio_dev;
	struct ad7606_state *st = iio_priv(indio_dev);
	int ret = 0;

	while (!kthread_should_stop()) {
		int got, max_frames;

		if (!atomic_read(&pd->capture_run)) {
			if (pd->hw_active) {
				ad7606_rp1_hw_capture_stop(pd);
				ad7606_rp1_pwm_stream_stop(pd);
			}
			pd->capture_idle = true;
			wake_up(&pd->idle_wait);
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(msecs_to_jiffies(20));
			continue;
		}

		pd->capture_idle = false;

		if (!iio_buffer_enabled(indio_dev)) {
			cond_resched();
			continue;
		}

		if (!ad7606_iio_chrdev_is_open(indio_dev)) {
			if (pd->hw_active) {
				ad7606_rp1_hw_capture_stop(pd);
				ad7606_rp1_pwm_stream_stop(pd);
			}
			wait_event_interruptible_timeout(
				pd->buffer_wait,
				ad7606_iio_chrdev_is_open(indio_dev) ||
				!atomic_read(&pd->capture_run) ||
				kthread_should_stop(),
				msecs_to_jiffies(AD7606_RP1_BUFFER_WAIT_MS));
			continue;
		}

		if (!pd->hw_active) {
			ret = ad7606_rp1_hw_capture_start(pd);
			if (ret) {
				dev_err_ratelimited(pd->dev,
						    "PIO capture start failed (%d)\n",
						    ret);
				msleep(20);
				continue;
			}
			ret = ad7606_rp1_pwm_stream_start(pd);
			if (ret) {
				ad7606_rp1_hw_capture_stop(pd);
				dev_err_ratelimited(pd->dev,
						    "CONVST start failed (%d)\n",
						    ret);
				msleep(20);
				continue;
			}
		}

		max_frames = ad7606_rp1_buffer_frames_available(indio_dev);
		if (max_frames == 0) {
			wait_event_interruptible_timeout(
				pd->buffer_wait,
				ad7606_rp1_buffer_has_space(indio_dev) ||
				!atomic_read(&pd->capture_run) ||
				!ad7606_iio_chrdev_is_open(indio_dev) ||
				kthread_should_stop(),
				msecs_to_jiffies(AD7606_RP1_BUFFER_WAIT_MS));
			continue;
		}

		got = ad7606_rp1_capture_burst(pd, indio_dev, st, max_frames);
		if (got > 0) {
			ret = 0;
			continue;
		}
		if (got < 0)
			ret = got;

		if (ret < 0)
			cond_resched();
		else
			cpu_relax();
	}

	if (pd->hw_active) {
		ad7606_rp1_hw_capture_stop(pd);
		ad7606_rp1_pwm_stream_stop(pd);
	}
	pd->capture_idle = true;
	wake_up(&pd->idle_wait);

	return 0;
}

static void ad7606_rp1_dma_free(struct ad7606_rp1_pio_data *pd)
{
	if (pd->dma_buf && pd->pio_dma_dev) {
		dma_free_coherent(pd->pio_dma_dev, pd->dma_buf_bytes,
				  pd->dma_buf, pd->dma_handle);
		pd->dma_buf = NULL;
		pd->dma_handle = 0;
	}

	if (pd->pio_dma_dev) {
		put_device(pd->pio_dma_dev);
		pd->pio_dma_dev = NULL;
	}
}

static int ad7606_rp1_stream_init(struct ad7606_rp1_pio_data *pd,
				  struct device *dev,
				  struct iio_dev *indio_dev)
{
	int ret;

	pd->dev = dev;
	pd->indio_dev = indio_dev;
	init_waitqueue_head(&pd->idle_wait);
	init_waitqueue_head(&pd->buffer_wait);
	pd->capture_idle = true;
	atomic_set(&pd->capture_run, 0);
	atomic64_set(&pd->dropped_frames, 0);

	pd->frame_buf = devm_kmalloc(dev,
				     AD7606_RP1_MAX_CHANNELS * sizeof(u16),
				     GFP_KERNEL);
	if (!pd->frame_buf)
		return -ENOMEM;

	if (ad7606_rp1_use_dma) {
		ret = ad7606_rp1_dma_alloc_buf(pd);
		if (!ret)
			pd->use_dma = true;
	}

	pd->capture_task = kthread_run(ad7606_rp1_capture_thread, pd,
				       "ad7606-cap");
	if (IS_ERR(pd->capture_task)) {
		ad7606_rp1_dma_free(pd);
		return PTR_ERR(pd->capture_task);
	}

	return 0;
}

static void ad7606_rp1_stream_fini(struct ad7606_rp1_pio_data *pd)
{
	if (!pd->capture_task)
		return;

	atomic_set(&pd->capture_run, 0);
	kthread_stop(pd->capture_task);
	pd->capture_task = NULL;
	ad7606_rp1_dma_free(pd);
}

static void ad7606_rp1_pio_hw_fini(struct ad7606_rp1_pio_data *pd)
{
	if (!pd || !pd->pio)
		return;

	if (pd->capture_task)
		ad7606_rp1_stream_fini(pd);

	if (pd->sm >= 0) {
		pio_sm_set_enabled(pd->pio, pd->sm, false);
		pio_sm_unclaim(pd->pio, pd->sm);
		pd->sm = -1;
	}
	if (pd->offset != PIO_ORIGIN_INVALID) {
		pio_remove_program(pd->pio, ad7606_rp1_pio_program(pd), pd->offset);
		pd->offset = PIO_ORIGIN_INVALID;
	}
	pio_close(pd->pio);
	pd->pio = NULL;
	pd->enabled = false;

	if (ad7606_rp1_active_pd == pd)
		ad7606_rp1_active_pd = NULL;
}

static void ad7606_rp1_pio_devm_release(void *data)
{
	struct ad7606_rp1_pio_data *pd = data;

	ad7606_rp1_pio_hw_fini(pd);
}

static int ad7606_rp1_hw_capture_start(struct ad7606_rp1_pio_data *pd)
{
	int ret;

	if (!pd || !pd->enabled)
		return -ENODEV;
	if (pd->hw_active)
		return 0;

	if (pd->dma_buf && ad7606_rp1_use_dma)
		pd->use_dma = true;

	if (!pd->use_dma && ad7606_rp1_use_dma) {
		ret = ad7606_rp1_dma_alloc_buf(pd);
		if (!ret)
			pd->use_dma = true;
	}

	ret = ad7606_rp1_hw_start(pd);
	if (ret)
		return ret;

	if (pd->use_dma) {
		u32 dmactrl = AD7606_RP1_DMACTRL_RX_DREQ_EN |
			      AD7606_RP1_DMACTRL_RX_DWELL(2) |
			      AD7606_RP1_DMACTRL_RX_THRESH(8);

		ret = pio_sm_config_xfer(pd->pio, pd->sm, PIO_DIR_FROM_SM,
					 AD7606_RP1_DMA_BUF_BYTES,
					 AD7606_RP1_DMA_XFER_BUFS);
		if (ret) {
			dev_warn(pd->dev, "PIO DMA config failed (%d), FIFO only\n",
				 ret);
		} else {
			ret = pio_sm_set_dmactrl(pd->pio, pd->sm, false, dmactrl);
			if (ret) {
				dev_warn(pd->dev, "PIO dmactrl DREQ_EN failed (%d)\n",
					 ret);
			} else {
				dev_info(pd->dev,
					 "PIO DMA active (buf %u bytes, handle %pad)\n",
					 pd->dma_buf_bytes, &pd->dma_handle);
			}
		}
	}

	pd->hw_active = true;
	return 0;
}

static void ad7606_rp1_hw_capture_stop(struct ad7606_rp1_pio_data *pd)
{
	u64 dropped;

	if (!pd || !pd->enabled || !pd->hw_active)
		return;

	ad7606_rp1_hw_stop(pd);
	pd->hw_active = false;

	dropped = atomic64_read(&pd->dropped_frames);
	if (dropped)
		dev_info(pd->dev, "capture paused, %llu frame(s) dropped\n",
			 dropped);
}

static int ad7606_rp1_pwm_stream_start(struct ad7606_rp1_pio_data *pd)
{
	struct ad7606_state *st = iio_priv(pd->indio_dev);

	return ad7606_pwm_set_swing(st);
}

static void ad7606_rp1_pwm_stream_stop(struct ad7606_rp1_pio_data *pd)
{
	struct ad7606_state *st = iio_priv(pd->indio_dev);

	ad7606_pwm_set_low(st);
}

void ad7606_rp1_reset_stream_counters(struct ad7606_rp1_pio_data *pd)
{
	if (!pd)
		return;

	atomic64_set(&pd->dropped_frames, 0);
	pd->misalign_events = 0;
	pd->dma_fail_count = 0;
	if (pd->pio && pd->sm >= 0)
		pio_sm_clear_fifos(pd->pio, pd->sm);
}
EXPORT_SYMBOL_GPL(ad7606_rp1_reset_stream_counters);

int ad7606_rp1_stream_session_arm(struct ad7606_rp1_pio_data *pd)
{
	if (!pd || !pd->enabled)
		return -ENODEV;

	atomic_set(&pd->capture_run, 1);
	pd->capture_idle = false;
	wake_up_process(pd->capture_task);

	return 0;
}
EXPORT_SYMBOL_GPL(ad7606_rp1_stream_session_arm);

void ad7606_rp1_stream_session_disarm(struct ad7606_rp1_pio_data *pd)
{
	if (!pd || !pd->enabled)
		return;

	atomic_set(&pd->capture_run, 0);
	wait_event_timeout(pd->idle_wait, pd->capture_idle,
			   msecs_to_jiffies(500));
	ad7606_rp1_hw_capture_stop(pd);
	ad7606_rp1_pwm_stream_stop(pd);
}
EXPORT_SYMBOL_GPL(ad7606_rp1_stream_session_disarm);

static int ad7606_rp1_pio_sm_init(struct ad7606_rp1_pio_data *pd)
{
	pio_sm_config c;
	int ret;

	ret = pio_gpio_init(pd->pio, AD7606_RP1_PIN_CS);
	if (ret)
		return ret;
	ret = pio_gpio_init(pd->pio, AD7606_RP1_PIN_MISO);
	if (ret)
		return ret;
	ret = pio_gpio_init(pd->pio, AD7606_RP1_PIN_SCLK);
	if (ret)
		return ret;

	ret = pio_sm_set_consecutive_pindirs(pd->pio, pd->sm, AD7606_RP1_PIN_CS, 1, true);
	if (ret)
		return ret;
	ret = pio_sm_set_consecutive_pindirs(pd->pio, pd->sm, AD7606_RP1_PIN_MISO, 1, false);
	if (ret)
		return ret;
	ret = pio_sm_set_consecutive_pindirs(pd->pio, pd->sm, AD7606_RP1_PIN_SCLK, 1, true);
	if (ret)
		return ret;

	c = ad7606_spi_read_default_config(pd->offset);
	sm_config_set_set_pins(&c, AD7606_RP1_PIN_CS, 1);
	sm_config_set_sideset_pins(&c, AD7606_RP1_PIN_SCLK);
	sm_config_set_in_pins(&c, AD7606_RP1_PIN_MISO);
	sm_config_set_in_shift(&c, true, true, 16);
	sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
	sm_config_set_clkdiv_int_frac(&c, ad7606_rp1_clkdiv, 0);

	ret = pio_sm_init(pd->pio, pd->sm, pd->offset, &c);
	if (ret)
		return ret;

	return pio_sm_set_enabled(pd->pio, pd->sm, false);
}

static int ad7606_rp1_pio_program_init(struct ad7606_rp1_pio_data *pd)
{
	return ad7606_rp1_pio_sm_init(pd);
}

int ad7606_rp1_pio_read_block(struct ad7606_rp1_pio_data *pd, int count, void *buf)
{
	unsigned short *data = buf;
	int i, level, ret;
	unsigned long deadline;

	if (!pd || !pd->enabled)
		return -ENODEV;

	count = min_t(int, count, pd->read_channels);

	deadline = jiffies + msecs_to_jiffies(50);
	do {
		level = ad7606_rp1_fifo_rx_level(pd);
		if (level < 0)
			return level;
		if (level >= pd->pio_words_per_frame)
			break;
		cpu_relax();
	} while (time_before(jiffies, deadline));

	if (level < (int)pd->pio_words_per_frame)
		return -ETIMEDOUT;

	if (ad7606_rp1_fifo_complete_frames(pd, level) < 1)
		return -ETIMEDOUT;

	for (i = 0; i < pd->pio_words_per_frame; i++) {
		u32 raw = pio_sm_get_blocking(pd->pio, pd->sm);

		if (i < count) {
			u16 sample = (u16)(raw >> 16);

			data[i] = swab16(sample);
		}
	}

	ret = pio_get_error(pd->pio);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(ad7606_rp1_pio_read_block);

int ad7606_rp1_register_sysfs(struct iio_dev *indio_dev)
{
	struct ad7606_state *st = iio_priv(indio_dev);

	if (!st->rp1_pio_en || !st->bus_data)
		return 0;

	return devm_device_add_group(&indio_dev->dev, &ad7606_rp1_sysfs_group);
}
EXPORT_SYMBOL_GPL(ad7606_rp1_register_sysfs);

int ad7606_rp1_pio_probe(struct device *dev, struct iio_dev *indio_dev)
{
	struct ad7606_state *st = iio_priv(indio_dev);
	struct ad7606_rp1_pio_data *pd;
	unsigned int ch;
	int ret, sm;

	if (!of_property_read_bool(dev_of_node(dev), "ad7606,rp1-pio-spi"))
		return 0;

	pd = devm_kzalloc(dev, sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return -ENOMEM;

	pd->sm = -1;
	pd->offset = PIO_ORIGIN_INVALID;

	pd->pio = pio_open();
	if (!pd->pio)
		return dev_err_probe(dev, -ENODEV, "failed to open PIO\n");

	ret = devm_add_action_or_reset(dev, ad7606_rp1_pio_devm_release, pd);
	if (ret)
		return ret;

	ch = ad7606_rp1_channels;
	if (ch < 1)
		ch = 1;
	if (ch > AD7606_RP1_MAX_CHANNELS)
		ch = AD7606_RP1_MAX_CHANNELS;
	ad7606_rp1_apply_channel_config(pd, ch);

	if (!pio_can_add_program(pd->pio, ad7606_rp1_pio_program(pd)))
		return dev_err_probe(dev, -ENOMEM, "no room for PIO program\n");

	pd->offset = pio_add_program(pd->pio, ad7606_rp1_pio_program(pd));
	if (pd->offset == PIO_ORIGIN_INVALID)
		return dev_err_probe(dev, -EIO, "failed to load PIO program\n");

	sm = pio_claim_unused_sm(pd->pio, false);
	if (sm < 0) {
		for (sm = 0; sm < NUM_PIO_STATE_MACHINES; sm++) {
			if (!pio_sm_is_claimed(pd->pio, sm)) {
				ret = pio_sm_claim(pd->pio, sm);
				if (!ret)
					break;
			}
		}
		if (sm >= NUM_PIO_STATE_MACHINES)
			return dev_err_probe(dev, -EBUSY, "no PIO SM available\n");
	}
	pd->sm = sm;

	ret = ad7606_rp1_pio_program_init(pd);
	if (ret)
		return dev_err_probe(dev, ret, "PIO program init failed\n");

	ret = ad7606_rp1_stream_init(pd, dev, indio_dev);
	if (ret)
		return dev_err_probe(dev, ret, "PIO stream init failed\n");

	pd->enabled = true;
	ad7606_rp1_active_pd = pd;
	st->bus_data = pd;
	st->rp1_pio_en = true;

	dev_info(dev,
		 "AD7606 SPI via RP1 PIO (SM%u, offset %u, %u ch, %u PIO words/frame, %s)\n",
		 pd->sm, pd->offset, pd->read_channels, pd->pio_words_per_frame,
		 pd->use_dma ? "DMA+FIFO stream" : "FIFO stream");
	return 0;
}
EXPORT_SYMBOL_GPL(ad7606_rp1_pio_probe);

void ad7606_rp1_pio_shutdown(struct device *dev)
{
	struct iio_dev *indio_dev = dev ? dev_get_drvdata(dev) : NULL;
	struct ad7606_state *st;
	struct ad7606_rp1_pio_data *pd = NULL;

	if (indio_dev) {
		st = iio_priv(indio_dev);
		pd = st->bus_data;
	}
	if (!pd)
		pd = ad7606_rp1_active_pd;
	if (!pd || !pd->pio) {
		if (pd && pd->dev)
			dev_info(pd->dev, "AD7606 PIO shutdown: already closed\n");
		return;
	}

	dev_info(pd->dev, "AD7606 PIO shutdown: closing SM%u\n", pd->sm);

	if (pd->enabled)
		ad7606_rp1_stream_session_disarm(pd);

	ad7606_rp1_pio_hw_fini(pd);

	if (indio_dev) {
		st = iio_priv(indio_dev);
		st->rp1_pio_en = false;
		st->bus_data = NULL;
	}
}
EXPORT_SYMBOL_GPL(ad7606_rp1_pio_shutdown);

void ad7606_rp1_pio_remove(struct device *dev)
{
	ad7606_rp1_pio_shutdown(dev);
}
EXPORT_SYMBOL_GPL(ad7606_rp1_pio_remove);

MODULE_SOFTDEP("pre: rp1-pio");

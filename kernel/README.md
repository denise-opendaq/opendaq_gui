# AD7606-8 — kernel IIO driver for Raspberry Pi 5 (RP1 PIO)

Out-of-tree Linux driver for **Analog Devices AD7606-8** on **Raspberry Pi 5**: SPI read via **RP1 PIO** state machine, **PWM CONVST** on GPIO12, streaming through **Industrial I/O (IIO)** buffer.

Replaces userspace PIO capture: one kernel path from ADC to `/dev/iio:deviceN`.

---

## Features

- **8× simultaneous 16-bit bipolar channels** (AD7606-8)
- **RP1 PIO SPI** — not the SoC SPI controller (`spi0` is disabled in overlay)
- **Burst DMA** from PIO RX FIFO (default) or FIFO-only fallback
- **Runtime channel count** (`pio_spi_channels` 1–8) — shorter SPI frame → higher sample rate
- **±5 V / ±10 V** range via `adi,range-gpios` (GPIO6)
- **Exclusive device access** — only one process may `open(/dev/iio:deviceN)`; PWM/capture run only while fd is open
- **Timestamp / counter reset** on buffer enable and when `sampling_frequency` changes (buffer off)
- Functional test script with GPIO stimulus and plot (`kernel_test_ad7606.py`)

---

## Hardware

### Pinout (overlay `ad7606-overlay-pio.dts`)

| Raspberry Pi GPIO | AD7606 / function        | Notes                          |
|-------------------|--------------------------|--------------------------------|
| **GPIO8**         | CS                       | PIO SM output                  |
| **GPIO9**         | DOUTA (MISO)             | PIO SM input                   |
| **GPIO11**        | SCLK                     | PIO SM sideset                 |
| **GPIO12**        | CONVST                   | `rp1_pwm0`, hardware trigger   |
| **GPIO25**        | BUSY                     | Falling-edge IRQ               |
| **GPIO5**         | RESET                    | Active-high pulse at probe     |
| **GPIO6**         | RANGE                    | Low = ±5 V, High = ±10 V       |

Power (fixed regulators in DT):

- **AVCC** — 5.0 V (`ad7606-avcc`)
- **VDRIVE** — 3.3 V (`ad7606-vdrive`)

### Test wiring (`kernel_test_ad7606.py`)

| Signal    | Connection                          |
|-----------|-------------------------------------|
| GPIO13    | AD7606 **AIN1** (IIO channel 0)     |
| AD7606 V8 | **GND** (IIO channel 7 sanity check) |
| CONVST    | GPIO12 (overlay, no extra wire)     |

Channels 1–6 are typically floating (may read noisy values — ignore in tests).

---

## Architecture

```
PWM (GPIO12) ──► CONVST ──► AD7606 conversion
                              │
BUSY (GPIO25) ────────────────┘
                              │
PIO SM: wait BUSY ↓ → SPI read (8×16 bit) → PIO RX FIFO
                              │
                    DMA burst / FIFO poll (kthread)
                              │
                    IIO kfifo buffer
                              │
              open(/dev/iio:device0) + read()
```

- **Bottleneck** is PIO SPI time per frame, not the IIO header `max_samplerate`.
- **IIO timestamps** are taken on **BUSY falling edge** (end of ADC conversion) via a kernel IRQ ring; if the ring is empty, the driver falls back to `iio_get_time_ns()` with PIO FIFO depth backfill. Throughput to userspace (`wall fs`) can still lag; timestamp span tracks **conversion rate**, not `read()` rate. Overflow/drops: `busy_ts_overflow` / `dropped_frames` (not sysfs yet).

---

## Sample rate limits (measured on Pi 5, `clkdiv=6`)

| `pio_spi_channels` | Requested fs (stable data) | Typical wall throughput |
|--------------------|----------------------------|-------------------------|
| **8**              | ≤ **28–30 kHz**            | ~**20–21 kHz** to userspace |
| **1**              | up to **200 kHz**          | ~**135–140 kHz** wall     |

Higher requested `sampling_frequency` does **not** speed up SPI; `acq_time` grows while data stays valid.

To read fewer channels at runtime:

```bash
IIO=/sys/bus/iio/devices/iio:device0
echo 0 | sudo tee $IIO/buffer/enable
echo 3 | sudo tee $IIO/pio_spi_channels    # 1–8, buffer must be off
```

Odd counts are rounded up to a 32-bit SPI boundary in PIO (extra clocks discarded on unpack).

---

## First-time installation

### Requirements

- Raspberry Pi **5** (RP1), 64-bit Raspberry Pi OS / kernel **6.18+** with `rp1-pio`
- `dtc`, kernel headers: `/lib/modules/$(uname -r)/build`
- Python 3 + `lgpio`, `matplotlib` for tests

### Steps

```bash
cd ~/kernel/ad7606

# Build modules + overlay, install to /lib/modules/... and /boot/firmware/overlays/
sudo make install

# Persistent boot config
grep -q '^dtoverlay=ad7606-pio' /boot/firmware/config.txt || \
  echo dtoverlay=ad7606-pio | sudo tee -a /boot/firmware/config.txt

# /etc/modprobe.d/ad7606.conf → options ad7606 dma=1 clkdiv=6 channels=8
sudo make install_modprobe_dma

sudo reboot
```

After reboot:

```bash
ls /sys/bus/iio/devices/iio:device0/name   # ad7606-8
dmesg | grep ad7606
```

---

## Build and reload

| Command | Description |
|---------|-------------|
| `make` / `make load_kernel` | Build and install `.ko` into `/lib/modules/...` |
| `make dtbo` | Build `ad7606-pio.dtbo` only |
| `sudo make reload` | Rebuild, install, modprobe, check IIO device |
| `sudo make reload DMA=0` | FIFO-only (no DMA) |
| `sudo ./reload_ad7606.sh` | Same as `make reload` |
| `sudo make uninstall` | Remove overlay entry, modules, modprobe.conf |

**Module parameters** (boot via `/etc/modprobe.d/ad7606.conf` or `modprobe ad7606 ...`):

| Parameter | Default | Description |
|-----------|---------|-------------|
| `dma` | `1` | PIO burst DMA (`0` = FIFO only) |
| `clkdiv` | `6` | PIO SM divider (~4.2 MHz SPI at 6). Lower = faster, risk of corruption |
| `channels` | `8` | Channels clocked on SPI at probe (1–8) |

```bash
# Runtime (requires module reload / reboot if already loaded)
cat /sys/module/ad7606/parameters/dma
cat /sys/module/ad7606/parameters/clkdiv
cat /sys/module/ad7606/parameters/channels
```

**Pi 5 note:** `rmmod ad7606` often fails while capture is active → **`sudo reboot`** after installing new `.ko` if reload script asks for it. Do **not** use `dtoverlay -r` on Pi 5 (known segfault) — reboot instead.

---

## IIO usage

Device: `/sys/bus/iio/devices/iio:device0`, char dev `/dev/iio:device0`.

### Voltage range (±5 V / ±10 V)

GPIO6 is configured in overlay as `adi,range-gpios`.

```bash
IIO=/sys/bus/iio/devices/iio:device0
cat $IIO/in_voltage_scale_available    # 0.152588 0.305176

echo 0 | sudo tee $IIO/buffer/enable
echo 0.152588 | sudo tee $IIO/in_voltage_scale   # ±5 V  (RANGE = low)
echo 0.305176 | sudo tee $IIO/in_voltage_scale   # ±10 V (RANGE = high)
```

Range is **common for all channels**. Change only with buffer disabled.

### Capture lifecycle (exclusive open)

1. Configure scan elements, `sampling_frequency`, `pio_spi_channels` (buffer **off**).
2. `echo 1 > buffer/enable` — arms buffer, **resets** stream counters; **PWM still off**.
3. `open(/dev/iio:device0)` — starts CONVST (PWM) and PIO capture; **only one** open allowed (`EBUSY` on second).
4. `read()` — stream samples.
5. `close(fd)` — stops PWM and PIO.
6. `echo 0 > buffer/enable` — only after fd closed (`EBUSY` if still open).

Rules:

- `sampling_frequency` writable only with buffer **disabled**.
- `pio_spi_channels` writable only with buffer **disabled**.

### Minimal capture example

```bash
IIO=/sys/bus/iio/devices/iio:device0
SCAN=$IIO/scan_elements
BUF=$IIO/buffer

echo 0 | sudo tee $BUF/enable
echo 28000 | sudo tee $IIO/sampling_frequency

for ch in 0 1 2 3 4 5 6 7; do
  echo 1 | sudo tee $SCAN/in_voltage${ch}_en
done
echo 1 | sudo tee $SCAN/in_timestamp_en
echo 8192 | sudo tee $BUF/length
echo 1024 | sudo tee $BUF/watermark

echo 1 | sudo tee $BUF/enable

sudo dd if=/dev/iio:device0 of=/tmp/capture.bin bs=24 count=1000

echo 0 | sudo tee $BUF/enable
```

`scan_bytes` = 24 for 8×int16 + int64 timestamp (enable channels may change this — read `scan_elements`).

---

## Sysfs reference

Base path (symlink):

```text
IIO=/sys/bus/iio/devices/iio:device0
```

The same files are also available under  
`/sys/devices/platform/ad7606-pio/iio:device0/`.

Char device (stream read): `/dev/iio:device0` (major/minor in `IIO/dev`, typically `510:0`).

---

### Module parameters (`/sys/module/ad7606/parameters/`)

Set at `modprobe` / in `/etc/modprobe.d/ad7606.conf`. **Read-only at runtime** — changing them requires reloading the module.

| File | R/W | Format | Default | Description |
|------|-----|--------|---------|-------------|
| `dma` | R | `Y` / `N` | `Y` | PIO FIFO burst DMA (`N` = FIFO poll only). |
| `clkdiv` | R | integer | `6` | PIO SM clock divider. Lower → faster SPI, risk of corruption on 8 ch. |
| `channels` | R | `1`…`8` | `8` | Channels clocked on SPI at module load. Runtime: `pio_spi_channels`. |

---

### IIO device — `$IIO/`

| File | R/W | Format | Description |
|------|-----|--------|-------------|
| `name` | R | string | Chip name, e.g. `ad7606-8`. |
| `dev` | R | `MAJ:MIN` | Char device number (e.g. `510:0`). |
| `sampling_frequency` | RW | integer, **Hz** | CONVST rate (PWM on GPIO12). Up to 200000 (driver header); real ceiling depends on `pio_spi_channels` and `clkdiv`. **Write only when `buffer/enable=0`**. |
| `in_voltage_scale` | RW | float, V/LSB | Shared scale for all channels. Valid values from `in_voltage_scale_available`. Toggles GPIO6 (RANGE). **Write when `buffer/enable=0`**. |
| `in_voltage_scale_available` | R | float list | `0.152588` → ±5 V; `0.305176` → ±10 V. |
| `oversampling_ratio` | RW | integer | AD7606 oversampling ratio. Valid: `1 2 4 8 16 32 64`. **OS pins are not wired in the `ad7606-pio` overlay** — sysfs exists but changing ratio **does not drive hardware** without `adi,oversampling-ratio-gpios` in DT. |
| `pio_spi_channels` | RW | `1`…`8` | How many channels to clock on SPI (shorter frame → higher fs). **Write when `buffer/enable=0`**. Odd N is rounded up to a 32-bit SPI boundary (extra bits discarded on unpack). |
| `in_voltageN_raw` | RW | integer | Single-shot read of channel N (`0`…`7`) in **direct mode**. Buffer must be off; do not use during buffer streaming. Signed 16-bit (−32768…32767). Volts: `raw * in_voltage_scale`. |
| `current_timestamp_clock` | RW | string | IIO timestamp clock (`realtime`, `monotonic`, …). RP1 stream stamps BUSY IRQ time in this clock domain (`ktime_get_ns()`). |

Standard entries: `uevent`, `power/`, `of_node`, `waiting_for_supplier`, `subsystem`.

---

### Scan elements — `$IIO/scan_elements/`

Describe the layout of **one record** in the buffer. For each channel `N` = `0`…`7` and timestamp:

| File | R/W | Format | Example | Description |
|------|-----|--------|---------|-------------|
| `in_voltageN_en` | RW | `0` / `1` | `1` | Include channel N in buffer scan mask. Change only when **`buffer/enable=0`**. |
| `in_voltageN_index` | R | integer | `0`…`7` | Field position in the binary record (`read()` byte order). |
| `in_voltageN_type` | R | IIO type string | `le:s16/16>>0` | Little-endian, **signed 16**, 16 storage bits, shift 0. |
| `in_timestamp_en` | RW | `0` / `1` | `1` | Include timestamp in scan. |
| `in_timestamp_index` | R | integer | `8` | Timestamp index (after 8×int16 when all channels enabled). |
| `in_timestamp_type` | R | IIO type string | `le:s64/64>>0` | Little-endian **signed 64**, nanoseconds. |

**Type string format (IIO):**  
`[be|le]:[u|s][bits]/[storagebits]>>[shift]`

**Record size (`scan_bytes`):** sum of `storagebits/8` for all enabled fields (with alignment). Example (8 channels + timestamp):

```text
8 × 2 bytes (int16) + 8 bytes (int64) = 24 bytes
```

`read()` byte order: ascending `*_index` (ch0…ch7, then timestamp).

**Decode voltage from buffer:**

```text
voltage_V = int16_le * in_voltage_scale
```

Timestamp: `int64_le` ns from capture start (synthetic: `t0 + n × (1e9 / sampling_frequency)`).

---

### Buffer — `$IIO/buffer/` (duplicate `$IIO/buffer0/`)

| File | R/W | Format | Description |
|------|-----|--------|-------------|
| `enable` | RW | `0` / `1` | `1` — enable kfifo, **reset** stream counters (preenable). PWM/PIO **do not** start until `open(/dev/iio:device0)`. `0` — disable; **only if fd is closed** (`EBUSY` otherwise). |
| `length` | RW | integer | Ring buffer size in **records** (not bytes). Minimum 2. Default after probe: 8192. |
| `watermark` | RW | integer | Minimum records in buffer before `poll()`/`read()` unblocks. |
| `data_available` | R | integer | Number of **records** (scan frames) ready to read. Stays 0 without an open fd. |
| `direction` | R | string | `in` — capture only. |

---

### Char device `/dev/iio:device0`

| Operation | Behavior |
|-----------|----------|
| `open` | Exclusive: second process → **`EBUSY`**. Starts PWM (CONVST) and PIO capture. |
| `read` | Binary data, size multiple of `scan_bytes`. Blocks on watermark / data availability. |
| `close` | Stops PWM and PIO. |
| `poll` | Wait until data ≥ watermark. |

---

### Trigger — `/sys/bus/iio/devices/trigger0/`

| File | R/W | Format | Description |
|------|-----|--------|-------------|
| `name` | R | string | `ad7606-8-dev0` — internal BUSY-line trigger. |

On the **RP1 PIO stream** path, capture uses kthread + DMA/FIFO, not userspace `trigger/current_trigger`. The trigger is registered for IIO compatibility; streaming only needs the buffer and `/dev/iio:device0`.

---

### When attributes may be changed

| Action | `buffer/enable` | fd open |
|--------|-----------------|---------|
| `sampling_frequency` | **0** | any |
| `in_voltage_scale` | **0** | any |
| `pio_spi_channels` | **0** | any |
| `scan_elements/*_en` | **0** | any |
| `buffer/enable` → 1 | — | not required |
| `buffer/enable` → 0 | — | **closed** |
| `open(/dev/...)` | **1** | — |
| capture (PWM) | **1** | **open** |

### Typical write errors

| errno | Cause |
|-------|-------|
| `EBUSY` (16) | Second `open`; `buffer/disable` with fd open; `sampling_frequency` / `pio_spi_channels` with `buffer/enable=1`. |
| `EINVAL` (22) | `buffer/enable=1` with no `scan_elements` enabled; invalid `pio_spi_channels`. |
| `ENODEV` | `pio_spi_channels` on a device without RP1 PIO. |

---

## Tests

### Functional test (GPIO square wave + plot)

```bash
cd ~/kernel/ad7606

# 8 channels, 28 kHz, 3 toggle cycles
sudo python3 kernel_test_ad7606.py 28000 3

# 1 channel, 200 kHz
IIO=/sys/bus/iio/devices/iio:device0
echo 0 | sudo tee $IIO/buffer/enable
echo 1 | sudo tee $IIO/pio_spi_channels
sudo python3 kernel_test_ad7606.py 200000 3
```

Output:

- `ad7606_test.csv`, `ad7606_test.png`
- **Timestamp fs** — from BUSY-edge IIO timestamps (conversion time)
- **wall fs** — real delivery rate (`n_samples / acq_time`)

Expect ch0 Δ ≈ 3 V (GPIO13 → 3.3 V), ch7 ≈ 0 V (tied to GND).

### Benchmark (throughput only)

```bash
sudo python3 kernel_bench_ad7606.py 28000
```

---

## Troubleshooting

| Symptom | Likely cause / fix |
|---------|-------------------|
| No `iio:device0` | `dtoverlay=ad7606-pio` in config.txt, reboot; `dmesg \| grep ad7606` |
| `Unknown symbol` after build | Kernel/module mismatch — rebuild on running kernel, reboot |
| `rmmod ad7606` fails | Reboot; use `reload` script unbind path or full reboot |
| Garbage / wrong channels above ~30 kHz (8 ch) | SPI limit — lower fs or `pio_spi_channels=1` |
| `acq_time` ≫ 3 s at high fs | Requested fs > wall throughput — normal; check **wall fs** |
| `buffer/enable` EINVAL | Enable at least one `scan_elements/in_voltage*_en` |
| `buffer/enable` EBUSY | Close `/dev/iio:device0` first |
| Second `open()` fails | Expected — exclusive access |
| `pio_spi_channels` EBUSY | `echo 0 > buffer/enable` first |
| Scale write fails | Buffer off; use `in_voltage_scale`, not per-channel path |
| `dtoverlay -r` segfault | Reboot; edit `config.txt` instead |

### Useful diagnostics

```bash
dmesg | grep -iE 'ad7606|pio dma'
lsmod | grep ad7606
cat /sys/bus/iio/devices/iio:device0/name
cat /sys/bus/iio/devices/iio:device0/pio_spi_channels
cat /sys/bus/iio/devices/iio:device0/buffer/data_available
```

---

## Project layout

| File | Role |
|------|------|
| `ad7606_core.c` | IIO core, PWM, buffer ops, scale, probe |
| `ad7606_rp1_pio.c` | RP1 PIO program, DMA, capture kthread, sysfs |
| `ad7606_spi.pio` / `ad7606_spi_pio.h` | PIO SPI sequencer (mode 3) |
| `ad7606_spi_iface.c` | SPI bus / platform glue |
| `ad7606_rp1_platform.c` | `ad7606-pio` platform device |
| `ad7606-overlay-pio.dts` | Device tree overlay |
| `kernel_test_ad7606.py` | Functional test + plot |
| `kernel_bench_ad7606.py` | Sample-rate benchmark |
| `scripts/ad7606-reload-runtime.sh` | Reload helper (`make reload`) |
| `reload_ad7606.sh` | User wrapper for reload |
| `ad7606-modprobe-dma.conf` | Template for `/etc/modprobe.d/ad7606.conf` |

### Regenerating PIO header

If `ad7606_spi.pio` changes:

```bash
make pio
# Copy generated opcodes from build/pio_gen.h into ad7606_spi_pio.h if needed
```

**Do not** remove bit-loop `[1]` delays in PIO for 8-channel operation — causes SPI corruption.

---

## License

GPL-2.0. The AD7606 IIO core is derived from the Analog Devices / Linux staging AD7606 driver.

Modified for Raspberry Pi 5 (RP1 PIO) by **Denis Erokhin** — [denis.erokhin@opendaq.com](mailto:denis.erokhin@opendaq.com).

#!/usr/bin/env python3
"""
AD7606 kernel IIO test — GPIO13 toggle on CH0, CH7 = GND, plot.

Wiring (same as userspace ad7606-acquire test):
  GPIO13  -> AD7606 AIN1 (channel 0)
  V8      -> GND         (channel 7)
  CONVST  -> GPIO12 PWM   (kernel overlay)

Requires:
  sudo modprobe ad7606 ad7606_spi
  sudo dtoverlay ad7606-pio
  sudo python3 kernel_test_ad7606.py [fs_hz] [cycles]

Env:
  AD7606_FS=200
  AD7606_CYCLES=3
  AD7606_GPIO=13
  AD7606_PLOT=/path/to.png
  AD7606_CSV=/path/to.csv
"""
from __future__ import annotations

import argparse
import datetime
import errno
import glob
import os
import re
import select
import struct
import sys
import time
from pathlib import Path

import lgpio
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_CSV = SCRIPT_DIR / "ad7606_test.csv"
DEFAULT_PNG = SCRIPT_DIR / "ad7606_test.png"
NUM_CH = 8
STIMULUS_GPIO = 13
FS_V_DEFAULT = 5.0
CODES = 32768.0


def find_ad7606_iio():
    for path in glob.glob("/sys/bus/iio/devices/iio:device*"):
        try:
            with open(os.path.join(path, "name"), encoding="utf-8") as f:
                name = f.read().strip()
            if name.startswith("ad7606"):
                return path, name
        except OSError:
            continue
    raise RuntimeError(
        "ad7606 IIO device not found.\n"
        "  cd ~/kernel/ad7606 && sudo ./reload_ad7606.sh\n"
        "If dtoverlay -r segfaults: reboot, then run reload again.\n"
        "Persistent: add 'dtoverlay=ad7606-pio' to /boot/firmware/config.txt"
    )


def read_sysfs(path: str) -> str:
    with open(path, encoding="utf-8") as f:
        return f.read().strip()


def write_sysfs(path: str, value: str) -> None:
    with open(path, "w", encoding="utf-8") as f:
        f.write(value if value.endswith("\n") else value + "\n")


def try_write_sysfs(path: str, value: str) -> bool:
    try:
        write_sysfs(path, value)
        return True
    except OSError as e:
        if e.errno in (errno.EACCES, errno.ENOENT):
            return False
        raise


def set_sampling_frequency(iio: str, fs_hz: int) -> bool:
    path = os.path.join(iio, "sampling_frequency")
    if try_write_sysfs(path, str(fs_hz)):
        return True
    print(
        f"warning: {path} not writable (PWM path may use driver default). "
        f"Rebuild ad7606.ko after driver update, or recording uses time-based halves.",
        file=sys.stderr,
    )
    return False


def read_sampling_frequency(iio: str) -> int | None:
    path = os.path.join(iio, "sampling_frequency")
    try:
        return int(float(read_sysfs(path).split()[0]))
    except (OSError, ValueError):
        return None


def resolve_buffer_paths(iio: str) -> tuple[str, str]:
    buf0 = os.path.join(iio, "buffer0")
    buf = os.path.join(iio, "buffer")
    if os.path.isdir(buf0):
        return buf0, buf
    return os.path.join(iio, "scan_elements"), buf


def buffer_disable(buf: str) -> None:
    try:
        write_sysfs(os.path.join(buf, "enable"), "0")
    except OSError:
        pass


def parse_type_bits(t: str) -> int:
    m = re.search(r"/(\d+)>>", t)
    if not m:
        raise RuntimeError(f"unexpected IIO type: {t!r}")
    return int(m.group(1))


def compute_scan_bytes(scan: str) -> tuple[int, dict[str, tuple[int, int]]]:
    elems: list[tuple[int, str, int]] = []
    for fn in os.listdir(scan):
        if not re.fullmatch(r"in_voltage\d+_en", fn):
            continue
        name = fn[:-3]
        if int(read_sysfs(os.path.join(scan, fn))) != 1:
            continue
        idx = int(read_sysfs(os.path.join(scan, f"{name}_index")))
        size = parse_type_bits(read_sysfs(os.path.join(scan, f"{name}_type"))) // 8
        elems.append((idx, name, size))

    ts_en = os.path.join(scan, "in_timestamp_en")
    if os.path.exists(ts_en) and int(read_sysfs(ts_en)) == 1:
        idx = int(read_sysfs(os.path.join(scan, "in_timestamp_index")))
        size = parse_type_bits(read_sysfs(os.path.join(scan, "in_timestamp_type"))) // 8
        elems.append((idx, "in_timestamp", size))

    if not elems:
        raise RuntimeError("no enabled scan elements")

    elems.sort(key=lambda x: x[0])
    off = 0
    layout: dict[str, tuple[int, int]] = {}
    for _, name, size in elems:
        if off % size:
            off += size - (off % size)
        layout[name] = (off, size)
        off += size

    align = max(s for _, _, s in elems)
    if off % align:
        off += align - (off % align)
    return off, layout


def disable_scan(scan: str) -> None:
    for fn in os.listdir(scan):
        if fn.endswith("_en"):
            try:
                write_sysfs(os.path.join(scan, fn), "0")
            except OSError:
                pass


def enable_all_voltage_scan(scan: str, buf: str, *, timestamp: bool = True,
                            watermark: int = 512) -> None:
    disable_scan(scan)
    for ch in range(NUM_CH):
        write_sysfs(os.path.join(scan, f"in_voltage{ch}_en"), "1")
    if timestamp:
        write_sysfs(os.path.join(scan, "in_timestamp_en"), "1")
    write_sysfs(os.path.join(buf, "length"), str(watermark * 4))
    try:
        write_sysfs(os.path.join(buf, "watermark"), str(watermark))
    except OSError:
        pass


def read_channel_scale(iio: str, ch: int) -> str | None:
    """Return raw IIO scale string (informational only)."""
    for p in (os.path.join(iio, f"in_voltage{ch}_scale"),
              os.path.join(iio, "in_voltage_scale")):
        if os.path.exists(p):
            try:
                return read_sysfs(p)
            except OSError:
                pass
    return None


def code_to_volts(code: int, fs_v: float = FS_V_DEFAULT) -> float:
    """Bipolar +/-fs_v full scale (RANGE=0 -> +/-5 V on this board)."""
    return code * fs_v / CODES


def compile_layout(layout: dict[str, tuple[int, int]], scan_bytes: int):
    """
    Pre-build a single struct.Struct for the entire scan record so the hot
    per-frame path is one struct.unpack() call instead of 9+ separate ones.
    Returns (struct_obj, [(result_index, ch_idx), ...], ts_result_index_or_None).
    """
    fields: list[tuple[int, str, int, bool]] = []  # (offset, fmt_char, ch_or_-1, is_ts)
    for name, (off, size) in layout.items():
        if name.startswith("in_voltage"):
            m = re.match(r"in_voltage(\d+)", name)
            if m:
                fields.append((off, "h" if size == 2 else "i", int(m.group(1)), False))
        elif name == "in_timestamp":
            fields.append((off, "Q" if size == 8 else "I", -1, True))

    fields.sort()  # by offset

    fmt = "<"
    pos = 0
    ch_order: list[tuple[int, int]] = []
    ts_idx: int | None = None
    slot = 0
    for off, fc, ch_idx, is_ts in fields:
        if off > pos:
            fmt += f"{off - pos}x"
        fmt += fc
        fmt_size = struct.calcsize(fc)
        if is_ts:
            ts_idx = slot
        else:
            ch_order.append((slot, ch_idx))
        pos = off + fmt_size
        slot += 1
    if pos < scan_bytes:
        fmt += f"{scan_bytes - pos}x"

    return struct.Struct(fmt), ch_order, ts_idx


def unpack_frame(frame, compiled) -> tuple[dict[int, int], int | None]:
    s, ch_order, ts_idx = compiled
    vals = s.unpack_from(frame)
    ch = {ch_idx: vals[i] for i, ch_idx in ch_order}
    ts_ns = vals[ts_idx] if ts_idx is not None else None
    return ch, ts_ns


def fill_tail(fd: int, tail: bytearray, want_bytes: int, timeout_s: float) -> bool:
    """Block in select() until tail has want_bytes, draining all available each wake-up."""
    deadline = time.monotonic() + timeout_s
    while len(tail) < want_bytes:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return False
        ready, _, _ = select.select([fd], [], [], min(remaining, 0.5))
        if not ready:
            continue
        # Drain everything available in a tight loop — one select per watermark batch
        while True:
            try:
                chunk = os.read(fd, max(want_bytes - len(tail), 4096 * 24))
                if chunk:
                    tail.extend(chunk)
                else:
                    break
            except BlockingIOError:
                break
    return True


def read_one_frame(fd: int, scan_bytes: int, tail: bytearray,
                   compiled, timeout_s: float = 2.0):
    if not fill_tail(fd, tail, scan_bytes, timeout_s):
        raise TimeoutError("no sample from IIO buffer")
    result = unpack_frame(tail, compiled)
    del tail[:scan_bytes]
    return result


def drain_buffer(fd: int, scan_bytes: int, duration_s: float) -> int:
    if duration_s <= 0:
        return 0
    dropped = 0
    tail = bytearray()
    end = time.monotonic() + duration_s
    while time.monotonic() < end:
        try:
            chunk = os.read(fd, scan_bytes * 32)
        except BlockingIOError:
            time.sleep(0.001)
            continue
        if not chunk:
            time.sleep(0.001)
            continue
        tail.extend(chunk)
        dropped += len(tail) // scan_bytes
        del tail[: (len(tail) // scan_bytes) * scan_bytes]
    return dropped


def record_toggle(
    dev: str,
    scan_bytes: int,
    compiled,
    *,
    fs_hz: int,
    cycles: int,
    gpio: int,
    time_based_halves: bool = False,
    fd: int = -1,
) -> list[dict]:
    samples_per_half = max(1, fs_hz // 2)
    half_duration_s = 0.5
    records: list[dict] = []
    h = lgpio.gpiochip_open(0)
    try:
        lgpio.gpio_claim_output(h, gpio, 0)
        own_fd = fd < 0
        if own_fd:
            fd = os.open(dev, os.O_RDONLY | os.O_NONBLOCK)
        tail = bytearray()
        try:
            for half in range(cycles * 2):
                level = half & 1
                lgpio.gpio_write(h, gpio, level)
                if time_based_halves:
                    end = time.monotonic() + half_duration_s
                    while time.monotonic() < end:
                        ch, ts_ns = read_one_frame(fd, scan_bytes, tail, compiled, timeout_s=0.5)
                        records.append({
                            "gpio13": level,
                            "ts_ns": ts_ns,
                            "ch": ch,
                        })
                else:
                    for _ in range(samples_per_half):
                        ch, ts_ns = read_one_frame(fd, scan_bytes, tail, compiled)
                        records.append({
                            "gpio13": level,
                            "ts_ns": ts_ns,
                            "ch": ch,
                        })
        finally:
            if own_fd:
                os.close(fd)
            lgpio.gpio_write(h, gpio, 0)
    finally:
        lgpio.gpiochip_close(h)

    if not records:
        return records

    t0 = None
    for r in records:
        if r["ts_ns"] is not None:
            t0 = r["ts_ns"]
            break
    if t0 is None:
        t0_ns = 0
        for i, r in enumerate(records):
            r["ts_ms"] = 1000.0 * i / fs_hz
    else:
        for r in records:
            ts = r["ts_ns"] if r["ts_ns"] is not None else t0
            r["ts_ms"] = (ts - t0) / 1e6
    return records


def write_csv(path: Path, records: list[dict], *, fs_v: float) -> None:
    with open(path, "w", encoding="utf-8") as f:
        f.write("# ch1-ch6 not wired (floating inputs read ~4 V — ignore)\n")
        f.write("ts_ms,gpio13,ch0,ch0_V,ch7,ch7_V\n")
        for r in records:
            c0 = r["ch"].get(0, 0)
            c7 = r["ch"].get(7, 0)
            f.write(
                f"{r['ts_ms']:.3f},{r['gpio13']},{c0},{code_to_volts(c0, fs_v):.4f},"
                f"{c7},{code_to_volts(c7, fs_v):.4f}\n"
            )


def mean_volts(records: list[dict], gpio_level: int, ch: int, fs_v: float) -> float:
    vals = [
        code_to_volts(r["ch"][ch], fs_v)
        for r in records
        if r["gpio13"] == gpio_level and ch in r["ch"]
    ]
    if not vals:
        return float("nan")
    return sum(vals) / len(vals)


def plot_records(records: list[dict], png_path: Path, *, fs_v: float, gpio: int,
                 acq_info: dict | None = None) -> None:
    ts = [r["ts_ms"] for r in records]
    gpio_vals = [r["gpio13"] for r in records]
    v = {
        ch: [code_to_volts(r["ch"][ch], fs_v) for r in records]
        for ch in range(NUM_CH)
        if all(ch in r["ch"] for r in records)
    }

    fig, axes = plt.subplots(3, 1, figsize=(11, 9), sharex=True)

    axes[0].step(ts, gpio_vals, where="post", color="#444444", linewidth=2)
    axes[0].set_ylim(-0.2, 1.2)
    axes[0].set_yticks([0, 1])
    axes[0].set_yticklabels(["0 V", "3.3 V"])
    axes[0].set_ylabel(f"GPIO{gpio}")
    axes[0].grid(True, alpha=0.3)
    axes[0].set_title(
        f"AD7606 kernel test (GPIO{gpio} -> CH0, CH7 = GND)  scale: +/-{fs_v:.0f} V"
    )

    if 0 in v and 7 in v:
        axes[1].plot(ts, v[0], label="ch0 (AI1 / GPIO13)", color="#1f77b4", linewidth=1.2)
        axes[1].plot(ts, v[7], label="ch7 (V8 / GND)", color="#ff7f0e", linewidth=1.2)
    axes[1].set_ylabel("Voltage (V)")
    axes[1].set_ylim(-0.3, fs_v * 0.75)
    axes[1].legend(loc="upper right")
    axes[1].grid(True, alpha=0.3)

    colors = plt.cm.tab10.colors
    for ch in (0, 7):
        if ch in v:
            axes[2].plot(ts, v[ch], label=f"ch{ch}", color=colors[ch % 10],
                         alpha=0.85, linewidth=0.9)
    axes[2].set_xlabel("Time (ms)")
    axes[2].set_ylabel("Voltage (V)")
    axes[2].set_ylim(-fs_v, fs_v)
    axes[2].legend(ncol=4, fontsize=8, loc="upper right")
    axes[2].grid(True, alpha=0.3)

    lo0 = mean_volts(records, 0, 0, fs_v)
    hi0 = mean_volts(records, 1, 0, fs_v)
    lo7 = mean_volts(records, 0, 7, fs_v)
    hi7 = mean_volts(records, 1, 7, fs_v)
    fig.text(
        0.01, 0.01,
        f"ch0: LOW={lo0:.3f} V  HIGH={hi0:.3f} V  (delta {hi0 - lo0:.3f} V)   |   "
        f"ch7 (GND): LOW={lo7:.3f} V  HIGH={hi7:.3f} V",
        fontsize=9, family="monospace",
    )

    if acq_info:
        fs_meas = acq_info.get("measured_fs")
        fs_wall = acq_info.get("effective_wall_fs")
        fs_str = f"{fs_meas:.1f} Hz" if fs_meas is not None else "n/a"
        wall_str = f"{fs_wall:.1f} Hz" if fs_wall is not None else "n/a"
        fig.text(
            0.01, 0.04,
            f"n={acq_info['n_samples']}  "
            f"fs_ts={fs_str}  wall_fs={wall_str}  "
            f"acq_time={acq_info['acq_s']:.3f} s  "
            f"first_sample={acq_info['first_sample_str']}",
            fontsize=9, family="monospace",
        )

    fig.tight_layout(rect=[0, 0.06, 1, 1])
    png_path.parent.mkdir(parents=True, exist_ok=True)
    try:
        fig.savefig(png_path, dpi=130)
    except PermissionError:
        alt = Path("/tmp/ad7606_test.png")
        fig.savefig(alt, dpi=130)
        print(f"Permission denied for {png_path}; saved {alt}", file=sys.stderr)
        raise
    finally:
        plt.close(fig)
    print(f"Plot saved: {png_path}")


def main() -> int:
    ap = argparse.ArgumentParser(description="AD7606 GPIO13 toggle + IIO plot test")
    ap.add_argument("fs_hz", nargs="?", type=int,
                    default=int(os.environ.get("AD7606_FS", "200")),
                    help="sampling frequency Hz (default 200)")
    ap.add_argument("cycles", nargs="?", type=int,
                    default=int(os.environ.get("AD7606_CYCLES", "3")),
                    help="GPIO toggle cycles (default 3)")
    ap.add_argument("--gpio", type=int, default=int(os.environ.get("AD7606_GPIO", "13")))
    ap.add_argument("--csv", type=Path, default=Path(os.environ.get("AD7606_CSV", str(DEFAULT_CSV))))
    ap.add_argument("--png", type=Path, default=Path(os.environ.get("AD7606_PLOT", str(DEFAULT_PNG))))
    args = ap.parse_args()

    if os.geteuid() != 0:
        print("warning: run as root (sudo) for GPIO + IIO buffer", file=sys.stderr)

    iio, name = find_ad7606_iio()
    dev = "/dev/" + os.path.basename(iio)
    scan, buf = resolve_buffer_paths(iio)
    print(f"IIO: {iio} ({name})")
    print(f"dev: {dev}")

    buffer_disable(buf)
    disable_scan(scan)
    fs_set = set_sampling_frequency(iio, args.fs_hz)
    effective_fs = read_sampling_frequency(iio) if fs_set else None
    if effective_fs is None:
        effective_fs = args.fs_hz if fs_set else None
    wm = max(8, (effective_fs or args.fs_hz) // 25)
    enable_all_voltage_scan(scan, buf, watermark=wm)
    scan_bytes, layout = compute_scan_bytes(scan)
    compiled = compile_layout(layout, scan_bytes)
    time_based = not fs_set
    if time_based:
        print(f"scan_bytes={scan_bytes}  time-based halves (0.5 s)  cycles={args.cycles}")
    else:
        print(f"scan_bytes={scan_bytes}  fs={effective_fs} Hz  cycles={args.cycles}")

    scale = read_channel_scale(iio, 0)
    if scale is not None:
        print(f"IIO scale (ch0): {scale}")

    fd = -1
    try:
        write_sysfs(os.path.join(buf, "enable"), "1")
        fd = os.open(dev, os.O_RDONLY | os.O_NONBLOCK)
        drained = drain_buffer(fd, scan_bytes, 0.15)
        if drained:
            print(f"drained {drained} startup frames")

        print(f"Recording GPIO{args.gpio} toggle ({args.cycles} cycles)...")
        t_acq_start = time.monotonic()
        t_wall_start = time.time()
        try:
            records = record_toggle(
                dev, scan_bytes, compiled,
                fs_hz=effective_fs or args.fs_hz,
                cycles=args.cycles,
                gpio=args.gpio,
                time_based_halves=time_based,
                fd=fd,
            )
        finally:
            buffer_disable(buf)
    finally:
        if fd >= 0:
            os.close(fd)
    t_acq_s = time.monotonic() - t_acq_start

    if not records:
        print("no samples captured", file=sys.stderr)
        return 1

    n = len(records)
    span_ms = records[-1]["ts_ms"] - records[0]["ts_ms"]
    # Timestamps: BUSY IRQ ring (conversion end); FIFO backfill fallback.
    measured_fs: float | None = (n - 1) / (span_ms / 1000.0) if span_ms > 0 else None
    effective_wall_fs: float | None = (n - 1) / t_acq_s if t_acq_s > 0 else None
    first_sample_str = datetime.datetime.fromtimestamp(t_wall_start).strftime(
        "%Y-%m-%d %H:%M:%S.%f"
    )[:-3]
    acq_info = {
        "n_samples": n,
        "measured_fs": measured_fs,
        "effective_wall_fs": effective_wall_fs,
        "requested_fs": effective_fs or args.fs_hz,
        "acq_s": t_acq_s,
        "expected_acq_s": float(args.cycles),
        "first_sample_str": first_sample_str,
    }

    print(f"Recorded {n} samples")
    if measured_fs is not None:
        print(
            f"Timestamp fs: {measured_fs:.1f} Hz  "
            f"wall fs: {effective_wall_fs:.1f} Hz  "
            f"acq_time: {t_acq_s:.3f} s"
        )
        if effective_wall_fs is not None and effective_fs:
            req = effective_fs
            if effective_wall_fs < req * 0.9:
                print(
                    f"warning: wall throughput {effective_wall_fs:.0f} Hz << requested {req:.0f} Hz "
                    f"(SPI/PIO limit for 8ch ~28-30 kHz; acq_time grows as userspace waits)",
                    file=sys.stderr,
                )
    print(f"First sample: {first_sample_str}")
    write_csv(args.csv, records, fs_v=FS_V_DEFAULT)
    print(f"CSV saved: {args.csv}")

    plot_records(records, args.png, fs_v=FS_V_DEFAULT, gpio=args.gpio, acq_info=acq_info)

    lo0 = mean_volts(records, 0, 0, FS_V_DEFAULT)
    hi0 = mean_volts(records, 1, 0, FS_V_DEFAULT)
    lo7 = mean_volts(records, 0, 7, FS_V_DEFAULT)
    hi7 = mean_volts(records, 1, 7, FS_V_DEFAULT)
    print(f"ch0: LOW={lo0:.3f} V  HIGH={hi0:.3f} V  delta={hi0 - lo0:.3f} V")
    print(f"ch7 (GND): LOW={lo7:.3f} V  HIGH={hi7:.3f} V")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

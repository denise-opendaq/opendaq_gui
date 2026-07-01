#!/usr/bin/env python3
"""Measure achieved AD7606 IIO sample rate vs requested sampling_frequency."""
import glob
import os
import select
import sys
import time

NUM_CH = 8


def find_iio():
    for path in glob.glob("/sys/bus/iio/devices/iio:device*"):
        with open(os.path.join(path, "name")) as f:
            if f.read().strip().startswith("ad7606"):
                return path
    raise SystemExit("ad7606 IIO not found")


def write_sysfs(path, val):
    with open(path, "w") as f:
        f.write(f"{val}\n")


def read_sysfs(path):
    with open(path) as f:
        return f.read().strip()


def scan_bytes(scan):
    import re
    elems = []
    for fn in os.listdir(scan):
        if not re.fullmatch(r"in_voltage\d+_en", fn):
            continue
        name = fn[:-3]
        if int(open(os.path.join(scan, fn)).read()) != 1:
            continue
        idx = int(open(os.path.join(scan, f"{name}_index")).read())
        t = open(os.path.join(scan, f"{name}_type")).read()
        m = re.search(r"/(\d+)>>", t)
        size = int(m.group(1)) // 8
        elems.append((idx, size))
    elems.sort()
    off = 0
    for _, size in elems:
        if off % size:
            off += size - (off % size)
        off += size
    align = max(s for _, s in elems)
    if off % align:
        off += align - (off % align)
    return off


def ensure_trigger(iio):
    trig = os.path.join(iio, "trigger", "current_trigger")
    if not os.path.exists(trig):
        return
    cur = read_sysfs(trig)
    if cur:
        return
    for path in glob.glob("/sys/bus/iio/devices/trigger*"):
        name_path = os.path.join(path, "name")
        if not os.path.isfile(name_path):
            continue
        name = read_sysfs(name_path)
        if name.startswith("ad7606"):
            write_sysfs(trig, name)
            return


def drain_dev(dev: str, scan_bytes: int, timeout_s: float = 0.25) -> None:
    fd = os.open(dev, os.O_RDONLY | os.O_NONBLOCK)
    end = time.monotonic() + timeout_s
    try:
        while time.monotonic() < end:
            try:
                os.read(fd, scan_bytes * 256)
            except BlockingIOError:
                time.sleep(0.01)
    finally:
        os.close(fd)


def bench_rate(iio, fs_hz, duration=1.0):
    scan = os.path.join(iio, "buffer0") if os.path.isdir(os.path.join(iio, "buffer0")) else os.path.join(iio, "scan_elements")
    buf = os.path.join(iio, "buffer")
    dev = "/dev/" + os.path.basename(iio)

    write_sysfs(os.path.join(buf, "enable"), "0")
    time.sleep(0.1)
    for ch in range(NUM_CH):
        write_sysfs(os.path.join(scan, f"in_voltage{ch}_en"), "1")
    write_sysfs(os.path.join(scan, "in_timestamp_en"), "0")
    write_sysfs(os.path.join(iio, "sampling_frequency"), str(fs_hz))
    time.sleep(0.1)
    # Large kfifo so capture thread is not starved while userspace drains.
    write_sysfs(os.path.join(buf, "length"), str(min(65536, max(4096, fs_hz * 2))))
    sb = scan_bytes(scan)
    ensure_trigger(iio)

    write_sysfs(os.path.join(buf, "enable"), "1")
    time.sleep(0.1)
    fd = os.open(dev, os.O_RDONLY | os.O_NONBLOCK)
    total = 0
    t0 = time.monotonic()
    try:
        while time.monotonic() - t0 < duration:
            try:
                chunk = os.read(fd, sb * 256)
            except BlockingIOError:
                chunk = b""
            if chunk:
                total += len(chunk) // sb
                continue
            select.select([fd], [], [], 0.005)
        wall = time.monotonic() - t0  # measure BEFORE cleanup
    finally:
        os.close(fd)
        write_sysfs(os.path.join(buf, "enable"), "0")
        time.sleep(0.15)
        drain_dev(dev, sb)
        time.sleep(0.15)

    return total, wall, total / wall if wall > 0 else 0.0


def main():
    if os.geteuid() != 0:
        print("run as root", file=sys.stderr)
        return 1

    iio = find_iio()
    rates = [1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000]
    if len(sys.argv) > 1:
        rates = [int(x) for x in sys.argv[1:]]

    print(f"{'requested':>10}  {'samples':>8}  {'achieved':>12}  {'eff%':>6}", flush=True)
    for fs in rates:
        try:
            n, wall, achieved = bench_rate(iio, fs)
            eff = 100.0 * achieved / fs if fs else 0
            print(f"{fs:10d}  {n:8d}  {achieved:12.1f}  {eff:5.1f}%", flush=True)
        except OSError as e:
            print(f"{fs:10d}  FAILED: {e}", flush=True)
            try:
                write_sysfs(os.path.join(iio, "buffer", "enable"), "0")
            except OSError:
                pass
            time.sleep(0.5)
            continue
        time.sleep(0.2)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

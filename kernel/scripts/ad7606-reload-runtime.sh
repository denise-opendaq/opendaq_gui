#!/bin/bash
# Pi 5 runtime: unload/reload modules, overlay probe, IIO device check.
# Called by: make reload  (see Makefile)
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DMA="${DMA:-1}"
DT_OVERLAY_TIMEOUT="${DT_OVERLAY_TIMEOUT:-15}"

OVERLAY="ad7606-pio"
DT_NODE="/proc/device-tree/ad7606-pio"
MODPROBE_CONF="/etc/modprobe.d/ad7606.conf"

dma_mode_enabled() {
	case "$DMA" in
	0|dma=0|fifo|nodma) return 1 ;;
	*) return 0 ;;
	esac
}

find_iio_dev() {
	local d name
	for d in /sys/bus/iio/devices/iio:device*; do
		[[ -d "$d" ]] || continue
		name="$(cat "$d/name" 2>/dev/null || true)"
		if [[ "$name" == ad7606-* ]]; then
			echo "$d"
			return 0
		fi
	done
	return 1
}

overlay_count() {
	dtoverlay -l 2>/dev/null | grep -c "$OVERLAY" || true
}

dt_node_present() {
	[[ -d "$DT_NODE" ]]
}

need_reboot() {
	local reason="${1:-Device tree or driver state is broken.}"

	echo "" >&2
	echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" >&2
	echo " REBOOT REQUIRED" >&2
	echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" >&2
	echo "$reason" >&2
	echo "" >&2
	echo "  sudo reboot" >&2
	echo "" >&2
	echo "After reboot, once:" >&2
	echo "  grep -q '^dtoverlay=$OVERLAY' /boot/firmware/config.txt || \\" >&2
	echo "    echo dtoverlay=$OVERLAY | sudo tee -a /boot/firmware/config.txt" >&2
	echo "  cd $DIR && sudo ./reload_ad7606.sh" >&2
	echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" >&2
	exit 1
}

unload_ad7606_modules() {
	local DEV i

	DEV="$(find_iio_dev || true)"
	if [[ -n "$DEV" && -f "$DEV/buffer/enable" ]]; then
		echo "disabling IIO buffer on $(basename "$DEV")..."
		echo 0 | sudo tee "$DEV/buffer/enable" >/dev/null 2>&1 || true
		sleep 1
	fi

	for i in 1 2 3; do
		if lsmod | grep -q '^ad7606_spi '; then
			echo "unloading ad7606_spi (attempt $i)..."
			if timeout 8 sudo rmmod ad7606_spi 2>/dev/null; then
				break
			fi
			sleep 1
		else
			break
		fi
	done
	if lsmod | grep -q '^ad7606_spi '; then
		need_reboot "rmmod ad7606_spi failed (PIO capture thread stuck?)."
	fi

	for i in 1 2 3; do
		if lsmod | grep -q '^ad7606 '; then
			echo "unloading ad7606 core (attempt $i)..."
			if timeout 8 sudo rmmod ad7606 2>/dev/null; then
				break
			fi
			sleep 1
		else
			break
		fi
	done
	if lsmod | grep -q '^ad7606 '; then
		need_reboot "rmmod ad7606 failed — stale module stays at old dma= setting; reboot required."
	fi
}

NOL="$(overlay_count)"
if [[ "$NOL" -gt 1 ]]; then
	echo "warning: $OVERLAY listed $NOL times in dtoverlay -l (broken) — reboot recommended"
fi

echo "== unload + reload AD7606 modules =="
unload_ad7606_modules
make -C "$DIR" load-ad7606 DMA="$DMA" || {
	dmesg | grep -i ad7606 | tail -5 >&2
	need_reboot "ad7606_spi failed to load (symbol mismatch with in-memory ad7606?)."
}

DMA_ACTUAL="$(cat /sys/module/ad7606/parameters/dma 2>/dev/null || echo '?')"
if dma_mode_enabled; then
	if [[ "$DMA_ACTUAL" != "Y" ]]; then
		echo "ERROR: requested dma=1 but /sys/module/ad7606/parameters/dma=$DMA_ACTUAL" >&2
		need_reboot "ad7606 was not reloaded with dma=1 (modprobe on an already-loaded module is a no-op)."
	fi
	echo "dma parameter: $DMA_ACTUAL (expect DMA+FIFO stream in dmesg after buffer enable)"
else
	if [[ "$DMA_ACTUAL" != "N" ]]; then
		echo "ERROR: requested FIFO but /sys/module/ad7606/parameters/dma=$DMA_ACTUAL" >&2
		need_reboot "ad7606 was not reloaded with dma=0 (modprobe on an already-loaded module is a no-op)."
	fi
	echo "dma parameter: $DMA_ACTUAL (FIFO stream only)"
fi

if dma_mode_enabled && [[ ! -f "$MODPROBE_CONF" ]]; then
	echo "warning: $MODPROBE_CONF missing — next boot may not use dma=1 on autoload" >&2
fi

reprobe_platform() {
	if [[ ! -d /sys/bus/platform/drivers/ad7606-rp1 ]]; then
		return 1
	fi
	if [[ ! -d /sys/devices/platform/ad7606-pio ]]; then
		return 1
	fi
	echo "re-binding platform ad7606-pio..."
	echo ad7606-pio | sudo tee /sys/bus/platform/drivers/ad7606-rp1/unbind >/dev/null 2>&1 || true
	sleep 1
	echo ad7606-pio | sudo tee /sys/bus/platform/drivers/ad7606-rp1/bind
}

DEV="$(find_iio_dev || true)"
if [[ -n "$DEV" ]]; then
	echo "OK: $(cat "$DEV/name") at $DEV"
else
	if dt_node_present; then
		echo "DT node present, waiting for probe..."
		sleep 2
		DEV="$(find_iio_dev || true)"
		if [[ -z "$DEV" ]]; then
			reprobe_platform || true
			sleep 2
			DEV="$(find_iio_dev || true)"
		fi
	fi
fi

if [[ -n "$DEV" ]]; then
	:
elif [[ "$NOL" -ge 1 ]] && ! dt_node_present; then
	echo "ERROR: overlay listed but $OVERLAY device-tree node is missing." >&2
	need_reboot
elif dt_node_present; then
	echo "ERROR: DT node present but AD7606 did not probe." >&2
	dmesg | grep -i ad7606 | tail -10 >&2
	need_reboot
else
	echo "== apply overlay: $OVERLAY (timeout ${DT_OVERLAY_TIMEOUT}s) =="
	make -C "$DIR" apply-overlay
	if ! timeout "$DT_OVERLAY_TIMEOUT" sudo dtoverlay \
		"/boot/firmware/overlays/${OVERLAY}.dtbo"; then
		echo "ERROR: dtoverlay hung or failed — reboot and add dtoverlay=$OVERLAY to config.txt" >&2
		exit 1
	fi
	sleep 2
	DEV="$(find_iio_dev || true)"
	if [[ -z "$DEV" ]]; then
		echo "ERROR: no AD7606 IIO device after overlay" >&2
		dmesg | grep -i ad7606 | tail -10 >&2
		exit 1
	fi
fi

echo "OK: $(cat "$DEV/name") at $DEV"
if [[ -f "$DEV/sampling_frequency" ]]; then
	echo "sampling_frequency: $(cat "$DEV/sampling_frequency")"
fi
if [[ -f /sys/module/ad7606/parameters/channels ]]; then
	echo "pio_channels (module default): $(cat /sys/module/ad7606/parameters/channels)"
fi
if [[ -n "${DEV:-}" && -f "$DEV/pio_spi_channels" ]]; then
	echo "pio_spi_channels: $(cat "$DEV/pio_spi_channels")"
elif [[ -f /sys/devices/platform/ad7606-pio/pio_spi_channels ]]; then
	echo "pio_spi_channels (platform): $(cat /sys/devices/platform/ad7606-pio/pio_spi_channels)"
fi
echo "Run: sudo python3 $DIR/kernel_test_ad7606.py 200 3"
echo "Set SPI channels (buffer off): echo N | sudo tee \$IIO/pio_spi_channels"

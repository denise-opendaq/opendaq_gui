#!/bin/bash
# Load AD7606 kernel modules + ad7606-pio device-tree overlay.
#
# Usage:
#   sudo ./reload_ad7606.sh          # ad7606-pio + DMA (default)
#   sudo ./reload_ad7606.sh fifo     # FIFO only (dma=0)
#
# Pi 5: never use dtoverlay -r (segfault). If reload fails -> reboot.
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DMA=1

case "${1:-}" in
0|dma=0|fifo|nodma) DMA=0 ;;
1|dma=1|dma|"") DMA=1 ;;
*)
	echo "Unknown option: $1" >&2
	echo "Usage: sudo $0 [fifo]" >&2
	exit 1
	;;
esac

exec make -C "$DIR" reload DMA="$DMA"

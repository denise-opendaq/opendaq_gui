# -------------------------------------------------------
# AD7606 kernel IIO driver + DTS overlay (RP1 PIO)
#
# ad7606-overlay-pio.dts — RP1 PIO SPI + PWM CONVST
#
# Raspberry Pi kernel has no CONFIG_SPI_OFFLOAD; offload code is compiled
# out. RP1 PIO capture uses ad7606_rp1_pio.c instead.
#
# Build artifacts go to build/; "make clean" removes that directory.
#
# Reload workflow (also: sudo ./reload_ad7606.sh):
#   make reload          DMA=1 by default
#   make reload DMA=0      FIFO only
# -------------------------------------------------------

BUILD := build

ifneq ($(KERNELRELEASE),)

ccflags-y += -DCONFIG_SPI_OFFLOAD=0

obj-m += ad7606.o
ad7606-y := ad7606_core.o ad7606_rp1_pio.o
obj-m += ad7606_spi.o
ad7606_spi-y := ad7606_spi_iface.o ad7606_rp1_platform.o

else

KDIR    := /lib/modules/$(shell uname -r)/build
PWD     := $(shell pwd)
CONFIG  := /boot/firmware/config.txt
MODPROBE_CONF := /etc/modprobe.d/ad7606.conf
MODDIR  := /lib/modules/$(shell uname -r)/kernel/drivers/iio/adc
OVERLAY := ad7606-pio
DTBO_FW := /boot/firmware/overlays/$(OVERLAY).dtbo
DT_OVERLAY_TIMEOUT ?= 15

# reload / reload_ad7606.sh: 1 = DMA (default), 0 = FIFO only
DMA ?= 1
CLKDIV ?= 6
CHANNELS ?= 8

all: dtbo load_kernel

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/.prepared: Makefile $(wildcard *.c *.h) | $(BUILD)
	@ln -sfn ../Makefile $(BUILD)/Makefile
	@for f in *.c *.h; do ln -sfn ../$$f $(BUILD)/; done
	@touch $(BUILD)/.prepared

$(BUILD)/ad7606-pio.dtbo: ad7606-overlay-pio.dts | $(BUILD)
	dtc -@ -I dts -O dtb ad7606-overlay-pio.dts -o $(BUILD)/ad7606-pio.dtbo

dtbo: $(BUILD)/ad7606-pio.dtbo

rebuild: clean dtbo modules
	@test -s $(BUILD)/ad7606.ko -a -s $(BUILD)/ad7606_spi.ko || \
		(echo "ERROR: build produced empty module(s) in $(BUILD)/" >&2; exit 1)

install_dtbo: dtbo
	@echo "Installing DT overlay..."
	sudo cp $(BUILD)/ad7606-pio.dtbo $(DTBO_FW)

apply-overlay: install_dtbo
	@echo "DT overlay copied to $(DTBO_FW)"

uninstall_dtbo:
	@echo "Unloading overlay..."
	@if sudo dtoverlay -l 2>/dev/null | grep -q "ad7606-pio"; then \
		sudo dtoverlay -r ad7606-pio 2>/dev/null || true; \
	fi
	@sudo rm -f /boot/firmware/overlays/ad7606-pio.dtbo /boot/firmware/overlays/ad7606.dtbo

modules: $(BUILD)/.prepared
	$(MAKE) -C $(KDIR) M=$(PWD)/$(BUILD) modules

load_kernel: modules
	@echo "Installing kernel modules..."
	sudo install -d $(MODDIR)
	sudo cp $(BUILD)/ad7606.ko $(BUILD)/ad7606_spi.ko $(MODDIR)/
	@if ! sudo depmod -a; then \
		echo "ERROR: depmod failed — retrying clean build" >&2; \
		$(MAKE) clean modules; \
		sudo cp $(BUILD)/ad7606.ko $(BUILD)/ad7606_spi.ko $(MODDIR)/; \
		sudo depmod -a; \
	fi

install_modprobe_dma:
	@echo "Installing $(MODPROBE_CONF) (dma=1 for boot autoload)..."
	sudo install -m 644 ad7606-modprobe-dma.conf $(MODPROBE_CONF)

uninstall_modprobe:
	@if [ -f "$(MODPROBE_CONF)" ]; then \
		echo "Removing $(MODPROBE_CONF) (FIFO mode)..."; \
		sudo rm -f $(MODPROBE_CONF); \
	fi

install_modprobe:
ifeq ($(DMA),1)
	$(MAKE) install_modprobe_dma
else
	$(MAKE) uninstall_modprobe
endif

modprobe-deps:
	sudo modprobe industrialio
	sudo modprobe industrialio-triggered_buffer
	sudo modprobe kfifo_buf
	sudo modprobe rp1-pio

$(BUILD)/ad7606_spi_pio.h: ad7606_spi.pio | $(BUILD)
	pioasm -o c-sdk ad7606_spi.pio $(BUILD)/pio_gen.h
	@echo "Regenerated $(BUILD)/pio_gen.h — copy opcodes into ad7606_spi_pio.h if changed"

pio: $(BUILD)/ad7606_spi_pio.h

load-ad7606: modprobe-deps
	@echo "loading ad7606 with dma=$(DMA) clkdiv=$(CLKDIV) channels=$(CHANNELS)..."
	sudo modprobe ad7606 dma=$(DMA) clkdiv=$(CLKDIV) channels=$(CHANNELS)
	sudo modprobe ad7606_spi

reload-install: rebuild load_kernel install_modprobe
	@echo "Modules installed (dma=$(DMA))"

reload:
	$(MAKE) reload-install DMA=$(DMA)
	@DMA=$(DMA) CLKDIV=$(CLKDIV) CHANNELS=$(CHANNELS) DT_OVERLAY_TIMEOUT=$(DT_OVERLAY_TIMEOUT) ./scripts/ad7606-reload-runtime.sh

unload_kernel:
	@if lsmod | grep -q "^ad7606_spi "; then sudo rmmod ad7606_spi 2>/dev/null || true; fi
	@if lsmod | grep -q "^ad7606 "; then sudo rmmod ad7606 2>/dev/null || true; fi
	sudo rm -f $(MODDIR)/ad7606.ko $(MODDIR)/ad7606_spi.ko
	sudo depmod -a

install: install_dtbo load_kernel install_modprobe_dma
	@echo ""
	@echo "Installed. sudo dtoverlay ad7606-pio"
	@echo "Persist:  dtoverlay=ad7606-pio + $(MODPROBE_CONF) in $(CONFIG)"
	@echo "Reboot once so boot autoload picks up dma=1."

uninstall: uninstall_dtbo unload_kernel uninstall_modprobe
	@sudo sed -i '/^dtoverlay=ad7606-pio/d' $(CONFIG) 2>/dev/null || true

clean:
	rm -rf $(BUILD)

.PHONY: all rebuild install uninstall install_dtbo apply-overlay uninstall_dtbo \
	install_modprobe install_modprobe_dma uninstall_modprobe modprobe-deps \
	load-ad7606 reload reload-install modules load_kernel unload_kernel dtbo pio clean

endif

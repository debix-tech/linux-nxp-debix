#
#SeekWave Platform Drivers Makefile
#
#
PWD :=$(shell pwd)
CURFOLDER ?=$(pwd)

ccflags-y :=-Idrivers/misc/seekwaveplatform/skwutil
ccflags-y +=-Idrivers/misc/seekwaveplatform/usb
ccflags-y +=-Idrivers/misc/seekwaveplatform/sdio



ifeq ($(CONFIG_SKW_BT),m)
        ccflags-y += -DCONFIG_BT_SEEKWAVE
endif

ifneq ($(skw_extra_flags),)
        ccflags-y += $(skw_extra_flags) -DSKW_EXT_INC
endif

ifeq ($(CONFIG_OF), y)
        #ccflags-y += -DCONFIG_NO_DTS
endif

ccflags-y += -DCONFIG_SV6160_LITE_FPGA

obj-$(CONFIG_SKW_SDIOHAL) += skw_sdio.o
skw_sdio-y := ./sdio/skw_sdio_main.o
skw_sdio-y += ./sdio/skw_sdio_rx.o
skw_sdio-y += ./sdio/skw_sdio_debugfs.o
skw_sdio-y += ./sdio/skw_sdio_log.o
skw_sdio-y += ./skwutil/skw_user_com.o
skw_sdio-y += ./skwutil/skw_log_to_file.o
skw_sdio-y += ./skwutil/skw_boot.o

obj-$(CONFIG_SKW_USB) += skw_usb.o
skw_usb-y := ./usb/skw_usb_io.o
skw_usb-y += ./usb/skw_usb_debugfs.o
skw_usb-y += ./usb/skw_usb_log.o
skw_usb-y += ./skwutil/skw_user_com.o
skw_usb-y += ./skwutil/skw_log_to_file.o
skw_usb-y += ./skwutil/skw_boot.o

clean:
	@rm -rf *.o *.ko *.mod.c *.order *.a *.builtin .*.cmd .*.d


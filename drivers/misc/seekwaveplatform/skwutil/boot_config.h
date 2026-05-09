/******************************************************************************
 *
 * Copyright(c) 2020-2030  Seekwave Corporation.
 *
 *****************************************************************************/
#ifndef __BOOT_CONFIG_H__
#define __BOOT_CONFIG_H__
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include "skw_boot.h"
#define  MODEM_ENABLE_GPIO   	-1
#define  HOST_WAKEUP_GPIO_IN 	-1
#define  MODEM_WAKEUP_GPIO_OUT  -1
#define  SEEKWAVE_NV_NAME  "SEEKWAVE_NV_SWT6621S.bin"
//#define CONFIG_SEEKWAVE_FIRMWARE_LOAD
#define  SKW_IRAM_FILE_PATH  "/data/ROM_EXEC_KERNEL_IRAM.bin"
#define  SKW_DRAM_FILE_PATH  "/data/RAM_RW_KERNEL_DRAM.bin"
#define  SKW_POWER_OFF_VALUE   0

/***********************************************************
**CONFIG_SKW_HOST_SUPPORT_ADMA 1 : use ADMA	0 : use SDMA
**
***********************************************************/
//#define CONFIG_SKW_HOST_SUPPORT_ADMA

#if defined(CONFIG_SKW_HOST_SUPPORT_ADMA)
#define TX_DMA_TYPE		TX_ADMA
#else
#define TX_DMA_TYPE		TX_SDMA
#endif
//#define CONFIG_SKW_HOST_PLATFORM_AMLOGIC 1

//#define  USB_POWEROFF_IN_LOWPOWER  1
#if defined(CONFIG_SKW_HOST_PLATFORM_AMLOGIC)
extern void extern_wifi_set_enable(int is_on);
#elif defined(CONFIG_SKW_HOST_PLATFORM_ALLWINER)
extern void sunxi_wlan_set_power(int on);
#elif defined(CONFIG_SKW_HOST_PLATFORM_ROCKCHIP)
extern int rockchip_wifi_power(int on);
#else
static inline int skw_chip_power_ops(int on){
    if(on){
		printk("skw self controll chip power on !!\n");
    }else{
		printk("skw self controll chip power down !!\n");
    }
	return 0;
}
#endif

static inline void skw_chip_set_power(int on)
{
#if defined(CONFIG_SKW_HOST_PLATFORM_AMLOGIC)
	extern_wifi_set_enable(on);
#elif defined(CONFIG_SKW_HOST_PLATFORM_ALLWINER)
	sunxi_wlan_set_power(on);
#elif defined(CONFIG_SKW_HOST_PLATFORM_ROCKCHIP)
	rockchip_wifi_power(on);
#else
	skw_chip_power_ops(on);
#endif

}
static inline void skw_chip_power_reset(void)
{
#if defined(CONFIG_SKW_HOST_PLATFORM_AMLOGIC)
	printk("amlogic skw chip power reset !!\n");
	extern_wifi_set_enable(0);
	msleep(50);
	extern_wifi_set_enable(1);
#elif defined(CONFIG_SKW_HOST_PLATFORM_ALLWINER)
	printk("allwinner skw chip power reset !!\n");
	sunxi_wlan_set_power(0);
	msleep(50);
	sunxi_wlan_set_power(1);
#elif defined(CONFIG_SKW_HOST_PLATFORM_ROCKCHIP)
	printk("rockchip skw chip power reset !!\n");
	rockchip_wifi_power(0);
	msleep(50);
	rockchip_wifi_power(1);
#else
	printk("self skw chip power reset !!\n");
	skw_chip_power_ops(0);
	msleep(50);
	skw_chip_power_ops(1);
#endif
}
//#define  STR_MODE_REINITBUS  1
#endif /* __BOOT_CONFIG_H__ */


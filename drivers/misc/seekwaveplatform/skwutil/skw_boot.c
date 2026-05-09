/*****************************************************************
 *Copyright (C) 2021 Seekwave Tech Inc.
 *Filename : skw_boot.c
 *Authors:seekwave platform
 *
 * This software is licensed under the terms of the the GNU
 * General Public License version 2, as published by the Free
 * Software Foundation, and may be copied, distributed, and
 * modified under those terms.
 *
 * This program is distributed in the hope that it will be usefull,
 * but without any warranty;without even the implied warranty of
 * merchantability or fitness for a partcular purpose. See the
 * GUN General Public License for more details.
 * **************************************************************/

#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/completion.h>
#include <linux/moduleparam.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/firmware.h>
#include <linux/mmc/sdio_func.h>
#include <linux/dma-mapping.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/scatterlist.h>
#include <linux/platform_device.h>
#include "sv6160_addr_map.h"
#include "skw_boot.h"
#include "boot_config.h"
/**************************sdio boot start******************************/
extern int cp_exception_sts;
int test_debug = 0;
module_param(test_debug, int, S_IRUGO);
unsigned char dl_signal_acount=0;
struct platform_device *btboot_pdev;
static u64 port_dmamask = DMA_BIT_MASK(32);
static int usb_chip_enable_gpio = -1;//32*4 + 8*3 + 4; //GPIO4_D4
static struct mutex boot_mutex;

//#define SDIO_BUFFER_SIZE	 (16*1024)
enum skw_sub_sys {
	SKW_BSP =1,
	SKW_WIFI,
	SKW_BLUETOOTH,
	SKW_ALL,
};

/*
 *add the little endian
 * */
#define _LITTLE_ENDIAN  1

#define CP_IMG_HEAD0	"kees"		 //"6B656573"
#define CP_IMG_HEAD1	"0616"		//"30363136"
#define CP_IMG_TAIL0	"evaw"		//"65766177"
#define CP_IMG_TAIL1	"0616"		//"30363136" //ASCII code 36 31 36 30
#define CP_NV_HEAD	  "TSVN"		//"5453564E" //ASCII code 36 31 36 30
#define CP_NV_TAIL		 "DEVN"		//"4445564E" //ASCII code 36 31 36 30

#define IMG_HEAD_OPS_LEN	4
#define RAM_ADDR_OPS_LEN	8
#define MODULE_INFO_LEN		12

#define IMG_HEAD_INFOR_RANGE	0x200  //10K Byte

unsigned int EndianConv_32(unsigned int value);
int skw_bind_boot_driver(struct device *dev);
/***********sdio drv extern interface **************/
/* driect mode,reg access.etc */
extern int skw_get_chipid(unsigned int system_addr, void *buf, unsigned int len);
extern int skw_boot_loader(struct seekwave_device *boot_data);
extern void *skw_get_bus_dev(void);
extern int skw_reset_bus_dev(void);
int skw_first_boot(struct seekwave_device *boot_data);
int skw_boot_init(struct seekwave_device *boot_data);
int skw_start_wifi_service(void);
int skw_start_bt_service(void);
int skw_stop_wifi_service(void);
int skw_stop_bt_service(void);
/**************************sdio boot end********************************/
struct seekwave_device *boot_data;
/***************************************************************************
 *Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 **************************************************************************/
#if !defined(CONFIG_SEEKWAVE_FIRMWARE_LOAD)
static unsigned int crc_16_l_calc(char *buf_ptr,unsigned int len)
{
	unsigned int i;
	unsigned short crc=0;

	while(len--!=0)
	{
		for(i= CRC_16_L_SEED;i!=0;i=i>>1)
		{
			if((crc &CRC_16_L_POLYNOMIAL)!=0)
			{
				crc= crc<<1;
				crc= crc ^ CRC_16_POLYNOMIAL;
			}else{
				crc = crc <<1;
			}

			if((*buf_ptr &i)!=0)
			{
				crc = crc ^ CRC_16_POLYNOMIAL;
			}
		}
		buf_ptr++;
	}
	return (crc);
}
static int skw_request_firmwares(struct seekwave_device *boot_data,
	const char *dram_image_name, const char *iram_image_name, const char *nv_mem_name)
{
	int ret;
	const struct firmware *fw;

	skwboot_log("firmware: %s %s \n", dram_image_name, iram_image_name);
	ret = request_firmware(&fw, dram_image_name, NULL);
	if (ret) {
		pr_err("request_firmware %s fail\n", dram_image_name);
		goto ret;
	}

	if (fw->size <= 0) {
		ret = -EINVAL;
		goto relese_fw;
	}

	boot_data->dram_img_data = (char *)vmalloc(fw->size);
	if (boot_data->dram_img_data == NULL) {
		pr_err("alloc dram memory failed\n");
		ret = -ENOMEM;
		goto relese_fw;
	}
	skwboot_log("boot data dram_img_data %p\n",boot_data->dram_img_data);
	memcpy(boot_data->dram_img_data, fw->data, fw->size);
	boot_data->dram_dl_size = fw->size;
	release_firmware(fw);
	//dram crc16
	boot_data->dram_crc_en = 1;
	boot_data->dram_crc_offset=0;
	boot_data->dram_crc_val = crc_16_l_calc(boot_data->dram_img_data + boot_data->dram_crc_offset, boot_data->dram_dl_size);

	ret = request_firmware(&fw, iram_image_name, NULL);
	if (ret) {
		pr_err("request_firmware %s fail ret %d\n", iram_image_name, ret);
		if (fw == NULL) {
			vfree(boot_data->dram_img_data);
			boot_data->dram_img_data = NULL;
			boot_data->dram_dl_size = 0;
			return ret;
		}
	}

	if (fw->size <= 0) {
		ret = -EINVAL;
		goto relese_fw;
	}

	boot_data->iram_img_data = (char *)vmalloc(fw->size);
	if (boot_data->iram_img_data == NULL) {
		pr_err("alloc iram memory failed\n");
		ret = -ENOMEM;
		goto relese_fw;
	}
	memcpy(boot_data->iram_img_data, fw->data, fw->size);
	boot_data->iram_dl_size = fw->size;
	release_firmware(fw);
	ret = 0;
	//iram crc16
	boot_data->iram_crc_en = 1;
	boot_data->iram_crc_offset=0;
	boot_data->iram_crc_val = crc_16_l_calc(boot_data->iram_img_data + boot_data->iram_crc_offset, boot_data->iram_dl_size);
	skwboot_log("boot data iram_img_data %p\n",boot_data->iram_img_data);

	ret = request_firmware(&fw, nv_mem_name, NULL);
	if (ret) {
		skwboot_err(" NV image request_firmware %s fail\n", nv_mem_name);
		ret = 0;
		goto ret;
	}

	boot_data->nv_mem_data = (char *)kzalloc(fw->size, GFP_KERNEL);
	if (boot_data->nv_mem_data == NULL) {
		skwboot_err("alloc nv memory failed\n");
		goto relese_fw;
	}
	memcpy(boot_data->nv_mem_data, fw->data, fw->size);
	boot_data->nv_mem_size = fw->size;
	ret=0;
	boot_data->nvmem_crc_en = 1;
	boot_data->nvmem_crc_offset=0;
	boot_data->nvmem_crc_val = crc_16_l_calc(boot_data->nv_mem_data + boot_data->nvmem_crc_offset, boot_data->nv_mem_size);

relese_fw:
	release_firmware(fw);
ret:
	return ret;
}
#endif

static int seekwave_boot_parse_dt(struct platform_device *pdev, struct seekwave_device *boot_data)
{
	int ret = 0;
	//enum of_gpio_flags flags;
	struct device_node *np = pdev->dev.of_node;
	/*add the dma type dts config*/
	if (of_property_read_u32(np, "bt_antenna", &(boot_data->bt_antenna))){
		skwboot_warn("no BT_antenna setting\n");
		boot_data->bt_antenna = 0;
	} else
		skwboot_log("BT_antenna setting: %d\n", boot_data->bt_antenna);

	if (of_property_read_u32(np, "dma_type", &(boot_data->dma_type))){
		boot_data->dma_type = ADMA;
		boot_data->chip_en = MODEM_ENABLE_GPIO; 
		boot_data->host_gpio =  HOST_WAKEUP_GPIO_IN;
		boot_data->chip_gpio =  MODEM_WAKEUP_GPIO_OUT;
		boot_data->skw_nv_name =  SEEKWAVE_NV_NAME;
		boot_data->iram_file_path =  SKW_IRAM_FILE_PATH;
		boot_data->dram_file_path =  SKW_DRAM_FILE_PATH;

		if (boot_data->chip_en >= 0)
			boot_data->chip_en = (boot_data->chip_en<<1) | SKW_POWER_OFF_VALUE;
		skwboot_warn("no DTS setting\n");
	} else {
		/*-add the iram img file path dts-*/
		ret = of_property_read_string(np, "skw_iram_path",(const char **)&(boot_data->iram_file_path));
		if(ret < 0){
			skwboot_err("%s:iram path fail ret=%d\n", __func__, ret);
		}
		/*-add the dram img file path dts-*/
		ret = of_property_read_string(np, "skw_dram_path",(const char **)&(boot_data->dram_file_path));
		if(ret < 0){
			skwboot_err("%s: dram path fail ret=%d\n",__func__,ret);
		}

		boot_data->host_gpio = of_get_named_gpio(np, "gpio_host_wake", 0);
		boot_data->chip_gpio = of_get_named_gpio(np, "gpio_chip_wake",0);
		boot_data->chip_en = 54;//John_gao of_get_named_gpio_flags(np, "gpio_chip_en",0, &flags);
		ret = of_property_read_string(np, "seekwave_nv_name",(const char **)&(boot_data->skw_nv_name));
		if(ret < 0){
			skwboot_err("%s:nv name get fail ret=%d\n",__func__, ret);
		}
		if (boot_data->chip_en >= 0)
			boot_data->chip_en = (boot_data->chip_en<<1)| 1;
	}
	if(test_debug==1){//test debug inband irq and nosleep en
		boot_data->chip_gpio=-1;
		boot_data->host_gpio=-1;
	}
	if (boot_data->host_gpio >= 0) {
		ret = devm_gpio_request_one(&pdev->dev, boot_data->host_gpio, GPIOF_IN, "HOST_WAKE" );
		if(ret < 0){
			gpio_free(boot_data->host_gpio);
			devm_gpio_request_one(&pdev->dev, boot_data->host_gpio, GPIOF_IN, "HOST_WAKE" );
		}
		if (boot_data->chip_gpio >= 0) {
			ret = devm_gpio_request_one(&pdev->dev, boot_data->chip_gpio, GPIOF_OUT_INIT_HIGH,"CHIP_WAKE");
			if (ret < 0)
				skwboot_err("%s:gpio_chip request fail ret=%d\n",__func__, ret);
			else
				gpio_set_value(boot_data->host_gpio, 1);
		}

	}

	if(boot_data->chip_gpio >= 0 && boot_data->host_gpio >=0) {
		boot_data->slp_disable = 0;
	} else {
		boot_data->slp_disable = 1;
	}
	if (boot_data->chip_en >= 0) {
		if (0x01)
			ret = devm_gpio_request_one(&pdev->dev, boot_data->chip_en>>1, GPIOF_OUT_INIT_LOW,"CHIP_EN");
		else
			ret = devm_gpio_request_one(&pdev->dev, boot_data->chip_en>>1, GPIOF_OUT_INIT_HIGH,"CHIP_EN");
	}

	skwboot_log("%s, chipen:%d gpio_out:%d gpio_in:%d ret=%d\n", __func__,boot_data->chip_en,
			boot_data->chip_gpio,boot_data->host_gpio, ret);
	return ret;
}

/************************************************************************/
//Description: BT start service
//Func: BT start service
//Call：
//Author:junwei.jiang
//Date:2021-11-1
//Modify:
/************************************************************************/
static int bt_start_service(int id, void *callback, void *data)
{
	int ret=0;
	if(cp_exception_sts)
		return -1;

	ret = skw_start_bt_service();
	if(ret < 0){
		skwboot_err("%s boot bt fail \n", __func__);
		return -1;
	}
	skwboot_log("%s line:%d  boot sucessfuly\n", __func__, __LINE__);
	return 0;
}

/************************************************************************/
//Description: BT stop service
//Func: BT stop service
//Call：
//Author:junwei.jiang
//Date:2021-11-1
//Modify:
/************************************************************************/
static int bt_stop_service(int id)
{
	int ret=0;

	if(cp_exception_sts)
		return 0;

	ret = skw_stop_bt_service();
	if(ret < 0){
		skwboot_err("%s boot bt fail \n", __func__);
		return -1;
	}
	skwboot_log("bt_stop_service OK\n");
	return 0;
}

/****************************************************************
 *Description:iram read the double img file
 *Func:
 *Calls:
 *Call By:sdio_dloader
 *Input:the file path
 *Output:download data and the data size dl_data image_size
 *Return：0:pass other fail
 *Others:
 *Author：JUNWEI.JIANG
 *Date:2022-02-07
 * **************************************************************/
#if defined(CONFIG_SEEKWAVE_FIRMWARE_LOAD)
static int skw_iram_img_read(struct seekwave_device *boot_data)
{
	struct file *filep=NULL ;

	mm_segment_t old_fs;
	int err = 0;

	if(!boot_data->iram_file_path){
		skwboot_err("iram_file_path NULL!!\n");
		return -1;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	filep = filp_open(boot_data->iram_file_path, O_RDONLY, 0);
	if (IS_ERR(filep)) {
		skwboot_err("%s: Failed to open the file %s\n", __func__, boot_data->iram_file_path);
		filep = filp_open(boot_data->iram_file_path, O_RDONLY, 0664);
		if (IS_ERR(filep)) {
			err = PTR_ERR(filep);
			skwboot_err("open file error, err = %d\n", err);
			goto fail;
		}
		skwboot_log("file bin path = %s\n", boot_data->iram_file_path);
	}
#if  LINUX_VERSION_CODE <= KERNEL_VERSION(4, 10, 0)
	struct kstat stat;
	err = vfs_stat(boot_data->iram_file_path, &stat);
	if (err) {
		skwboot_err("%s:%s vfs_stat fail !\n", __func__,boot_data->iram_file_path);
		goto fail;
	}
	boot_data->iram_dl_size = (int)stat.size;
	if (boot_data->iram_dl_size == 0) {
		skwboot_err("%s:iram dl size fail ! imgsize=0x%x\n", __func__,
				boot_data->iram_dl_size);
		goto fail;
	}
#else
	boot_data->iram_dl_size = filep->f_inode->i_size;
	if (boot_data->iram_dl_size == 0) {
		skwboot_err("%s:iram dl size fail ! imgsize=0x%x\n", __func__,
				boot_data->iram_dl_size);
		goto fail;
	}
	skwboot_log("file bin iram_dl_size = %d \n", boot_data->iram_dl_size);
#endif

	boot_data->iram_img_data = (char *)vmalloc(boot_data->iram_dl_size);
	if (boot_data->iram_img_data == NULL) {
		goto fail1;
	}
	if (skw_read_file(filep, boot_data->iram_img_data,boot_data->iram_dl_size, &filep->f_pos) != boot_data->iram_dl_size) {
		goto fail1;
	}

	set_fs(old_fs);
	filp_close(filep, NULL);
	return 0;
fail:
	if (!IS_ERR(filep) && filep != NULL) {
		filp_close(filep, NULL);
		skwboot_err("%s: analysis the done - '%s' \n",__func__, boot_data->iram_file_path);
	}
	set_fs(old_fs);
	return -3;
fail1:
	if (!IS_ERR(filep)){
		filp_close(filep, NULL);
	}
	set_fs(old_fs);
	return -1;
}
#endif
/****************************************************************
 *Description:dram read the double img file
 *Func:
 *Calls:
 *Call By:sdio_dloader
 *Input:the file path
 *Output:download data and the data size dl_data image_size
 *Return：0:pass other fail
 *Others:
 *Author：JUNWEI.JIANG
 *Date:2022-02-07
 * **************************************************************/
#if defined(CONFIG_SEEKWAVE_FIRMWARE_LOAD)
static int skw_dram_img_read(struct seekwave_device *boot_data)
{
	struct file *filep=NULL ;
	mm_segment_t old_fs;
	int error = 0;

	if(!boot_data->dram_file_path){
		skwboot_err("dram_file_path NULL!!\n");
		return -1;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	filep = filp_open(boot_data->dram_file_path, O_RDONLY, 0);
	if (IS_ERR(filep)) {
		skwboot_err("%s: Failed to open the file %s\n", __func__, boot_data->dram_file_path);
		filep = filp_open(boot_data->dram_file_path, O_RDONLY, 0664);
		if (IS_ERR(filep)) {
			error = PTR_ERR(filep);
			skwboot_err("open file error, err = %d\n", error);
			goto fail;
		}
		skwboot_log("file bin path = %s\n", boot_data->dram_file_path);
	}
#if  LINUX_VERSION_CODE <= KERNEL_VERSION(4, 10, 0)
	struct kstat stat;
	error = vfs_stat(boot_data->dram_file_path, &stat);
	if (error) {
		skwboot_err("%s: get the img size fail the vfs_stat fail -- %s \n", __func__,boot_data->dram_file_path);
		goto fail;
	}
	boot_data->dram_dl_size = (int)stat.size;
	if (boot_data->dram_dl_size == 0) {
		skwboot_err("%s:dram dl size fail ! imgsize=0x%x\n", __func__,
				boot_data->dram_dl_size);
		goto fail;
	}
#else
	boot_data->dram_dl_size = filep->f_inode->i_size;
	if (boot_data->dram_dl_size == 0) {
		skwboot_err("%s:iram dl size fail ! imgsize=0x%x\n", __func__,
				boot_data->dram_dl_size);
		goto fail;
	}
#endif
	skwboot_log("file bin dram_dl_size = %d \n", boot_data->dram_dl_size);
	boot_data->dram_img_data = (char *)vmalloc(boot_data->dram_dl_size);
	if (boot_data->dram_img_data == NULL) {
		goto fail1;
	}
	if (skw_read_file(filep, boot_data->dram_img_data,boot_data->dram_dl_size, &filep->f_pos) != boot_data->dram_dl_size) {
		goto fail1;
	}

	set_fs(old_fs);
	filp_close(filep, NULL);
	return 0;
fail:
	if (!IS_ERR(filep) && filep != NULL) {
		filp_close(filep, NULL);
		skwboot_err("%s: analysis the done - '%s' \n",__func__, boot_data->dram_file_path);
	}
	set_fs(old_fs);
	return -3;
fail1:
	if (!IS_ERR(filep)){
		filp_close(filep, NULL);
	}
	set_fs(old_fs);
	return -1;
}
#endif
/***************************************************************************
 *Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 **************************************************************************/
static int seekwave_boot_probe(struct  platform_device *pdev)
{
	int ret;
	int time_count=0;
	void *io_bus;
	struct device *dev=NULL;

	boot_data = devm_kzalloc(&pdev->dev, sizeof(struct seekwave_device), GFP_KERNEL);
	if (!boot_data) {
		skwboot_err("%s :kzalloc error !\n", __func__);
		return -ENOMEM;
	}
	mutex_init(&boot_mutex);
	seekwave_boot_parse_dt(pdev, boot_data);
	io_bus = skw_get_bus_dev();
	if (!io_bus) {

		skwboot_log("%s :CHIP_RESET AGAIN!\n", __func__);
		SKW_CHIP_POWEROFF(boot_data->chip_en);
		msleep(20);
		SKW_CHIP_POWERON(boot_data->chip_en);
		do {
			msleep(10);
			io_bus = skw_get_bus_dev();
		} while(!io_bus && time_count++ < 50);
	} else
		usb_chip_enable_gpio = -1;
	if (!io_bus) {
		skwboot_err("%s get bus dev fail !\n",__func__);
		return -ENODEV;
	}
	dev = io_bus;
	if (!strncmp(dev->bus->name, "usb", 3)) {
		boot_data->iram_file_path = "SWT6621_IRAM_USB.bin";
		boot_data->dram_file_path = "SWT6621_DRAM_USB.bin";
	} else {
		boot_data->iram_file_path = "SWT6621_IRAM_SDIO.bin";
		boot_data->dram_file_path = "SWT6621_DRAM_SDIO.bin";
	}
	skw_boot_init(boot_data);
#ifdef STR_MODE_REINITBUS
	boot_data->pdev = pdev;
#endif
	printk("bus type: %s\n", dev->bus->name);
	if (usb_chip_enable_gpio >=0 && dev->type->name
	    && !strncmp(dev->bus->name, "usb", 3))
		boot_data->chip_en = usb_chip_enable_gpio;
	ret = skw_first_boot(boot_data);
	if (strncmp(dev->bus->name, "usb", 3))
		skw_bind_boot_driver(io_bus);
	return ret;
}
/***************************************************************************
 *Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 **************************************************************************/
static void seekwave_boot_remove(struct  platform_device *pdev)
{
	skwboot_log("%s the Enter \n", __func__);

	if (btboot_pdev) {
		platform_device_unregister(btboot_pdev);
		btboot_pdev = NULL;
	}
	if(boot_data){
		if(boot_data->iram_img_data){
			vfree(boot_data->iram_img_data);
			boot_data->iram_img_data = NULL;
		}
		if(boot_data->dram_img_data){
			vfree(boot_data->dram_img_data);
			boot_data->dram_img_data = NULL;
		}

		boot_data->iram_file_path = NULL;
		boot_data->dram_file_path = NULL;
		devm_kfree(&pdev->dev, boot_data);
		boot_data=NULL;
	}
	mutex_destroy(&boot_mutex);
	return ;
}
extern void skw_modem_log_stop_rec(void);
static void seekwave_boot_shutdown(struct platform_device *pdev)
{
	printk("%s enter ...\n", __func__);
	skw_modem_log_stop_rec();
	skw_reset_bus_dev();
}
static const struct of_device_id seekwave_match_table[] ={

	{ .compatible = "seekwave,sv6160"},
	{ },
};

static struct platform_driver seekwave_driver ={

	.driver = {
		.owner = THIS_MODULE,
		.name  = "sv6160",
		.of_match_table = seekwave_match_table,
	},
	.probe = seekwave_boot_probe,
	.remove = seekwave_boot_remove,
	.shutdown = seekwave_boot_shutdown,
};

/***********************************************************************
 *Description:BT download boot pdata
 *Seekwave tech LTD
 *Author:junwei.jiang
 *Date:2021-11-3
 *Modify:
 ***********************************************************************/
struct sv6160_platform_data boot_pdata = {
	.data_port = 8,
	.bus_type = SDIO_LINK,
	.max_buffer_size = 0x800,
	.align_value = 4,
	.open_port = bt_start_service,
	.close_port = bt_stop_service,
};

/***************************************************************
 *Description:BT bind boot driver
 *Seekwave tech LTD
 *Author:junwei.jiang
 *Date:2021-11-3
 *Modify:
***************************************************************/
int skw_bind_boot_driver(struct device *dev)
{
	struct platform_device *pdev;
	char	pdev_name[32];
	int ret = 0;
	sprintf(pdev_name, "skw_ucom");
/*
 *	creaete BT DATA device
 */
	if(!dev){
		skwboot_err("%s the dev fail \n", __func__);
		return -1;
	}
	pdev = platform_device_alloc(pdev_name, PLATFORM_DEVID_AUTO);
	if(!pdev)
		return -ENOMEM;
	pdev->dev.parent = dev;
	pdev->dev.dma_mask = &port_dmamask;
	pdev->dev.coherent_dma_mask = port_dmamask;
	boot_pdata.port_name = "BTBOOT";
	boot_pdata.data_port = 8;
	ret = platform_device_add_data(pdev, &boot_pdata, sizeof(boot_pdata));
	if(ret) {
		dev_err(dev, "failed to add boot data \n");
		platform_device_put(pdev);
		return ret;
	}
	ret = platform_device_add(pdev);
	if(ret) {
		platform_device_put(pdev);
		skwboot_err("%s,line:%d the device add fail \n",__func__,__LINE__);
		return ret;
	}
	btboot_pdev = pdev;
	return ret;
}
/****************************************************************
 *Description:the data Little Endian process interface
 *Func:EndianConv_32
 *Calls:None
 *Call By:The img data process
 *Input:value
 *Output:the Endian data
 *Return：value
 *Others:
 *Author：JUNWEI.JIANG
 *Date:2021-08-26
 * **************************************************************/
unsigned int EndianConv_32(unsigned int value)
{
#ifdef _LITTLE_ENDIAN
	unsigned int nTmp = (value >>24 | value <<24);
	nTmp |= ((value >> 8) & 0x0000FF00);
	nTmp |= ((value << 8) & 0x00FF0000);
	return nTmp;
#else
	return value;
#endif
}

/****************************************************************
 *Description:dram read the double img file
 *Func:
 *Calls:
 *Call By:
 *Input:the file path
 *Output:download data and the data size dl_data image_size
 *Return：0:pass other fail
 *Others:
 *Author：JUNWEI.JIANG
 *Date:2022-02-07
 * **************************************************************/
int skw_download_signal_ops(void)
{
	unsigned int tmp_signal = 0;
	//download done flag ++
	dl_signal_acount ++;
	tmp_signal = dl_signal_acount;
	boot_data->dl_done_signal = 0xff&tmp_signal;
	boot_data->dl_acount_addr = SKW_SDIO_PD_DL_AP2CP_BSP;

	//gpio need set high or low power interrupt to cp wakeup
	boot_data->gpio_out = boot_data->chip_gpio;
	if(boot_data->gpio_val)
		boot_data->gpio_val =0;
	else
		boot_data->gpio_val =1;
	skwboot_log("%s line:%d download data ops done \n", __func__, __LINE__);
	return 0;
}

/****************************************************************
 *Description:analysis the double img dram iram
 *Func:
 *Calls:
 *Call By:
 *Input:the file path
 *Output:download data and the data size dl_data image_size
 *Return：0:pass other fail
 *Others:
 *Author：JUNWEI.JIANG
 *Date:2022-02-07
 * **************************************************************/
int skw_boot_init(struct seekwave_device *boot_data)
{
	int i =0;
	int k =0;
	unsigned int head_offset=0;
	unsigned int tail_offset=0;
	int ret = 0;
	struct img_head_data_t dl_data_info;
	unsigned int *data=NULL;
	unsigned int *nvdata=NULL;
	unsigned int *dl_addr_data=NULL;
#if defined(CONFIG_SEEKWAVE_FIRMWARE_LOAD)
	//read the iram data from file
	//boot_data->iram_file_path = "/vendor/etc/firmware/ROM_EXEC_KERNEL_IRAM.bin";
	ret = skw_iram_img_read(boot_data);
	if(ret !=0){
		skwboot_err("%s:read iram file fail \n", __func__);
		//return -1;
		boot_data->iram_img_data = NULL;
		boot_data->iram_dl_size = 0;
	}
	//read the dram data from file
	//boot_data->dram_file_path = "/vendor/etc/firmware/RAM_RW_KERNEL_DRAM.bin";
	ret = skw_dram_img_read(boot_data);
	if(ret !=0){
		skwboot_err("%s:read dram file fail \n", __func__);
		boot_data->dram_img_data = NULL;
		boot_data->dram_dl_size = 0;
	}
#else
	ret = skw_request_firmwares(boot_data, boot_data->dram_file_path,
		boot_data->iram_file_path, boot_data->skw_nv_name);
	if (ret < 0){
		ret = skw_request_firmwares(boot_data, "RAM_RW_KERNEL_DRAM.bin",
			"ROM_EXEC_KERNEL_IRAM.bin",boot_data->skw_nv_name);
		if(ret <0)
			return ret;
	}
	skwboot_log("image_size=%d,%d, ret=%d\n", boot_data->iram_dl_size, boot_data->dram_dl_size, ret);
#endif
	boot_data->head_addr = 0;
	boot_data->tail_addr = 0;
	boot_data->bsp_head_addr = 0;
	boot_data->bsp_tail_addr = 0;
	boot_data->wifi_head_addr =0;
	boot_data->wifi_tail_addr = 0;
	boot_data->bt_head_addr = 0;
	boot_data->bt_tail_addr = 0;
	boot_data->nv_head_addr = 0;
	boot_data->nv_tail_addr = 0;
	boot_data->nv_data_size = 0;

	if(boot_data->iram_img_data!=NULL){
		/*analysis the img*/
		for(i=0; i*IMG_HEAD_OPS_LEN<IMG_HEAD_INFOR_RANGE; i++)
		{
			if(!head_offset)
			{
				if((0==memcmp(CP_IMG_HEAD0, boot_data->iram_img_data+i*IMG_HEAD_OPS_LEN,IMG_HEAD_OPS_LEN))&&
						(0==memcmp(CP_IMG_HEAD1,boot_data->iram_img_data+(i+1)*IMG_HEAD_OPS_LEN,IMG_HEAD_OPS_LEN)))
					head_offset = (i+1)*IMG_HEAD_OPS_LEN;
			}else if(!tail_offset){
				if((0==memcmp(CP_IMG_TAIL0, boot_data->iram_img_data+i*IMG_HEAD_OPS_LEN, IMG_HEAD_OPS_LEN))&&
						(0==memcmp(CP_IMG_TAIL1, boot_data->iram_img_data+(i+1)*IMG_HEAD_OPS_LEN, IMG_HEAD_OPS_LEN))){
					tail_offset = (i-1)*IMG_HEAD_OPS_LEN;
					break;
				}
			}
		}

		/*analysis the nv*/
		for(k=0; k*IMG_HEAD_OPS_LEN<IMG_HEAD_INFOR_RANGE; k++)
		{
			if(!boot_data->nv_head_addr)
			{
				if(0==memcmp(CP_NV_HEAD, boot_data->iram_img_data+k*IMG_HEAD_OPS_LEN,IMG_HEAD_OPS_LEN))
					boot_data->nv_head_addr = k*IMG_HEAD_OPS_LEN;
			}else if(!boot_data->nv_tail_addr){
				if((0==memcmp(CP_NV_TAIL, boot_data->iram_img_data+k*IMG_HEAD_OPS_LEN, IMG_HEAD_OPS_LEN))){
					boot_data->nv_tail_addr = k*IMG_HEAD_OPS_LEN;
					boot_data->nv_data_size = boot_data->nv_tail_addr - boot_data->nv_head_addr - IMG_HEAD_OPS_LEN;
					nvdata = (u32 *) &boot_data->iram_img_data[boot_data->nv_head_addr];
					print_hex_dump(KERN_ERR, "nv data ", 0, 16, 1,nvdata, boot_data->nv_data_size+8, 1);
					break;
				}
			}
		}
		if(!tail_offset){
			skwboot_err("%s,%d,the iram_img not need analysis!!! or Fail!! \n",__func__,__LINE__);
			//boot_data->iram_img_data = NULL;
			return -1;
		}else{
			//get the iram img addr and dram img addr
			dl_addr_data = (unsigned int *)(boot_data->iram_img_data+head_offset+IMG_HEAD_OPS_LEN);
			boot_data->iram_dl_addr = dl_addr_data[0];
			boot_data->dram_dl_addr = dl_addr_data[1];
			head_offset = head_offset+RAM_ADDR_OPS_LEN;//jump the ram addr data;

			skwboot_log("%s line:%d,the tail_offset ---0x%x, the head_offset --0x%x ,iram_addr=0x%x,dram_addr=0x%x, \
					nv_head_addr:0x%x,nv_tail_addr:0x%x,nv_size=%d\n",__func__, __LINE__,tail_offset, head_offset,
					boot_data->iram_dl_addr,boot_data->dram_dl_addr,boot_data->nv_head_addr,boot_data->nv_tail_addr,
					boot_data->nv_data_size);
		}
		/*need download the img bin for WIFI or BT service dl_module >0*/
		head_offset = head_offset +IMG_HEAD_OPS_LEN;
		/*get the img head tail offset*/
		boot_data->head_addr = head_offset;
		boot_data->tail_addr = tail_offset;

		skwboot_log("%s line:%d analysis the img module\n", __func__, __LINE__);
		for(i=0; i*MODULE_INFO_LEN<=(tail_offset-head_offset); i++)
		{
			data = (unsigned int *)(boot_data->iram_img_data +head_offset+i*MODULE_INFO_LEN);
			dl_data_info.dl_addr=data[0];
			dl_data_info.write_addr =data[2];
			dl_data_info.index = 0x000000FF&EndianConv_32(data[1]);
			dl_data_info.data_size = 0x00FFFFFF&data[1];
			skwboot_log("%s line:%d dl_addr=0x%x, write_addr=0x%x, index=0x%x,data_size=0x%x\n", __func__,
					__LINE__, dl_data_info.dl_addr,dl_data_info.write_addr,dl_data_info.index,dl_data_info.data_size);

		}
		if(boot_data->nv_mem_size && (boot_data->nv_mem_size <= boot_data->nv_data_size)){
			memcpy((boot_data->iram_img_data+boot_data->nv_head_addr+4),boot_data->nv_mem_data,boot_data->nv_mem_size);
			kfree(boot_data->nv_mem_data);
			boot_data->nv_mem_data = NULL;
		}

	 }
	 return 0;
}

/***************************************************************************
 *Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 **************************************************************************/
int skw_start_wifi_service(void)
{
	int ret =0;

	skwboot_log("%s Enter cp_state =%d \n",__func__, cp_exception_sts);
	mutex_lock(&boot_mutex);
	boot_data->service_ops = SKW_WIFI_START;
	boot_data->dl_module = SKW_WIFI_BOOT;
	boot_data->first_dl_flag = 1;
	//download done flag ++
	skw_download_signal_ops();
	ret = skw_boot_loader(boot_data);
	mutex_unlock(&boot_mutex);
	if(ret !=0)
	{
		skwboot_err("%s,line:%d boot fail \n", __func__,__LINE__);
		return -1;
	}

	skwboot_log("%s wifi boot sucessfull\n", __func__);
	return 0;
}
EXPORT_SYMBOL_GPL(skw_start_wifi_service);

/***************************************************************************
 *Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 **************************************************************************/
int skw_stop_wifi_service(void)
{
	int ret =0;
	skwboot_log("%s Enter cp_state =%d \n",__func__, cp_exception_sts);
	mutex_lock(&boot_mutex);
	boot_data->service_ops = SKW_WIFI_STOP;
	boot_data->dl_module = 0;
	boot_data->first_dl_flag = 1;
	//download done flag ++
	//gpio need set high or low power interrupt to cp wakeup
	boot_data->gpio_out = boot_data->chip_gpio;
	if(boot_data->gpio_val)
		boot_data->gpio_val =0;
	else
		boot_data->gpio_val =1;
	ret = skw_boot_loader(boot_data);
	mutex_unlock(&boot_mutex);
	if(ret !=0)
	{
		skwboot_err("dload the img fail \n");
		return -1;
	}
	skwboot_log("seekwave boot stop done:%s\n",__func__);
	return 0;
}
EXPORT_SYMBOL_GPL(skw_stop_wifi_service);


/***************************************************************************
 *Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 **************************************************************************/
int skw_start_bt_service(void)
{
	int ret=0;
	skwboot_log("%s Enter cp_state =%d \n",__func__, cp_exception_sts);
	mutex_lock(&boot_mutex);
	boot_data->service_ops = SKW_BT_START;
	boot_data->first_dl_flag = 1;
	boot_data->dl_module = SKW_BT_BOOT;
	//download done flag ++
	skw_download_signal_ops();
	ret = skw_boot_loader(boot_data);
	mutex_unlock(&boot_mutex);
	if(ret !=0)
	{
		skwboot_err("%s boot fail \n", __func__);
		return -1;
	}

	skwboot_log("%s line:%d , boot bt sucessfully!\n", __func__,__LINE__);
	return 0;
}
EXPORT_SYMBOL_GPL(skw_start_bt_service);

/***************************************************************************
 *Description:
 *Seekwave tech LTD
 *Author:
 *Date:
 *Modify:
 **************************************************************************/

int skw_stop_bt_service(void)
{
	int ret =0;
	skwboot_log("%s Enter cp_state =%d \n",__func__, cp_exception_sts);
	mutex_lock(&boot_mutex);
	boot_data->service_ops = SKW_BT_STOP;
	boot_data->first_dl_flag = 1;
	//download done flag ++
	boot_data->dl_module = 0;
	//gpio need set high or low power interrupt to cp wakeup
	boot_data->gpio_out = boot_data->chip_gpio;
	if(boot_data->gpio_val)
		boot_data->gpio_val =0;
	else
		boot_data->gpio_val =1;
	ret = skw_boot_loader(boot_data);
	mutex_unlock(&boot_mutex);
	if(ret < 0)
	{
		skwboot_err("dload the img fail \n");
		return -1;
	}
	skwboot_log("seekwave boot stop done:%s\n",__func__);
	return 0;
}
EXPORT_SYMBOL_GPL(skw_stop_bt_service);

/****************************************************************
 *Description:double iram dram img first boot cp
 *Func:
 *Calls:
 *Call By:skw_first_boot
 *Input:the file path
 *Output:download data and the data size dl_data image_size
 *Return：0:pass other fail
 *Others:
 *Author：JUNWEI.JIANG
 *Date:2022-02-07
 * **************************************************************/
int skw_first_boot(struct seekwave_device *boot_data)
{
	int ret =0;
	//get the img data
#ifdef DEBUG_SKWBOOT_TIME
	ktime_t cur_time,last_time;
	cur_time = ktime_get();
#endif
	//set download the value;
	boot_data->service_ops = SKW_NO_SERVICE;
	boot_data->save_setup_addr = SKW_SDIO_PD_DL_AP2CP_BSP; //160
	boot_data->gpio_out = boot_data->chip_gpio;
	boot_data->gpio_val = 0;
	boot_data->dl_module = 0;
	boot_data->first_dl_flag =0;
	boot_data->gpio_in  = boot_data->host_gpio;
	boot_data->dma_type_addr = SKW_SDIO_PLD_DMA_TYPE;
	boot_data->slp_disable_addr = SKW_SDIO_CP_SLP_SWITCH;

	ret = skw_boot_loader(boot_data);
	if(ret < 0){
		skwboot_err("%s firt boot cp fail \n", __func__);
		return -1;
	}
	//download done set the download flag;
	boot_data->first_dl_flag =1;

	//download done tall cp acount;
	boot_data->dl_done_signal &= 0xFF;
	boot_data->dl_done_signal +=1;
	skwboot_log("%s first boot pass\n", __func__);
#ifdef DEBUG_SKWBOOT_TIME
	last_time = ktime_get();
	skwboot_log("%s,the download time start time %llu and the over time %llu \n",
			__func__, cur_time, last_time);
#endif
	return ret;
}

#ifdef CONFIG_NO_DTS
#undef CONFIG_OF
#endif

#ifndef CONFIG_OF
static void seekwave_release(struct device *dev)
{
}
static struct platform_device seekwave_device ={
	.name = "sv6160",
	.dev = {
		.release = seekwave_release,
	}
};
#endif
int seekwave_boot_init(void)
{
	btboot_pdev = NULL;
	skw_ucom_init();
#ifndef CONFIG_OF
	printk("NO DTS CONFIG BOOT INIT !!!\n");
	platform_device_register(&seekwave_device);
#endif
	return platform_driver_register(&seekwave_driver);
}

void seekwave_boot_exit(void)
{
	skw_ucom_exit();
#ifndef CONFIG_OF
	platform_device_unregister(&seekwave_device);
#endif
	platform_driver_unregister(&seekwave_driver);

}



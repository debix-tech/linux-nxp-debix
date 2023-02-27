/*
 * A V4L2 driver for GalaxyCore gc2145 cameras.
 *
 */

#include <asm/unaligned.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>
 

//////////////////John_gao/////////////////////////////////
#define gc2145_CHIP_ID		0x2145

#define gc2145_FRAME_RATE		30
#define gc2145_REG_VALUE_8BIT		1
#define gc2145_WIDTH_MAX		1600
#define gc2145_HEIGHT_MAX		1200
#define ov_info(fmt, args...)   pr_info("gc2145: "fmt, ##args)

#if 0
#define GLS(args...)   ov_info(args)
#else
#define GLS(args...)
#endif


enum gc2145_mode_id {
	//gc2145_MODE_QUXGA_800_600,
	gc2145_MODE_720P_1280_720,
	//gc2145_MODE_UXGA_1600_1200,
	gc2145_MODE_MAX,
};

struct reg_value {
	u8 reg_addr;
	u8 val;
};

struct gc2145_mode_info {
	const char *name;
	enum gc2145_mode_id id;
	u32 width;
	u32 height;
	const struct reg_value *reg_data;
	u32 reg_data_size;
};


struct gc2145_ctrls {
	struct v4l2_ctrl_handler handler;
	struct {
		struct v4l2_ctrl *auto_exp;
		struct v4l2_ctrl *exposure;
	};
	struct {
		struct v4l2_ctrl *auto_gain;
		struct v4l2_ctrl *gain;
	};

	struct v4l2_ctrl *hflip;
	struct v4l2_ctrl *vflip;
	struct v4l2_ctrl *test_pattern;
};

struct gc2145_dev {
	struct i2c_client		*i2c_client;
	struct v4l2_subdev		sd;

	struct media_pad		pad;
	struct clk			*xclk;
	u32				xclk_freq;
//	struct regulator_bulk_data	supplies[gc2145_NUM_SUPPLIES];

	//struct gpio_desc		*reset_gpio;
	//struct gpio_desc		*powdn_gpio; //add by soar at 20210906
	int reset_gpio;	//add by soar at 20210906
	int powdn_gpio;	//add by soar at 20210906
//	int dovdd_gpio;	//add by soar at 20210906
//	int avdd_gpio;	//add by soar at 20210906
	struct mutex			lock; /* protect members */

	bool				mode_pending_changes;
//	bool				is_enabled;
	bool				is_streaming;

	struct gc2145_ctrls		ctrls;
	struct v4l2_mbus_framefmt	fmt;
	struct v4l2_fract		frame_interval;

	const struct gc2145_mode_info	*current_mode;
};

static const char * const test_pattern_menu[] = {
	"Disabled",
	"Color Bars",
	"Random Data",
	"Square",
	"Black Image",
};

static const int gc2145_hv_flip_bayer_order[] = {
	//	MEDIA_BUS_FMT_YUYV8_2X8,
	//	MEDIA_BUS_FMT_YVYU8_2X8,
		MEDIA_BUS_FMT_UYVY8_2X8,
//		MEDIA_BUS_FMT_VYUY8_2X8,
		MEDIA_BUS_FMT_SBGGR8_1X8,
};

static struct reg_value sensor_fmt_yuv422_uyvy[] = {
    	{0xfe, 0x00},
	{0x84, 0x00}, //output put foramat

};

static struct reg_value gc2145_start_settings[] = {
	{0xfe, 0x03},
	{0x10, 0x94},
	{0xfe, 0x00},
};

static struct reg_value gc2145_stop_settings[] = {
	{0xfe, 0x03},
	{0x10, 0x84},
	{0xfe, 0x00},
};

#if 0 //not use
//for preview setting
static struct reg_value gc2145_svga_settings[] = {
    	{0xfe, 0x00},
	{0xfd, 0x01},
	{0xfa, 0x00},
	//// crop window             
	{0xfe , 0x00},
	{0x99 , 0x11},  
	{0x9a , 0x06},
	{0x9b , 0x00},
	{0x9c , 0x00},
	{0x9d , 0x00},
	{0x9e , 0x00},
	{0x9f , 0x00},
	{0xa0 , 0x00},  
	{0xa1 , 0x00},
	{0xa2  ,0x00},
	{0x90 , 0x01}, 
	{0x91 , 0x00},
	{0x92 , 0x00},
	{0x93 , 0x00},
	{0x94 , 0x00},
	{0x95 , 0x02},
	{0x96 , 0x58},
	{0x97 , 0x03},
	{0x98 , 0x20},

	//// AWB                      
	{0xfe , 0x00},
	{0xec , 0x02}, 
	{0xed , 0x02},
	{0xee , 0x30},
	{0xef , 0x48},
	{0xfe , 0x02},
	{0x9d , 0x0b},
	{0xfe , 0x01},
	{0x74 , 0x00}, 
	//// AEC                      
	{0xfe , 0x01},
	{0x01 , 0x04},
	{0x02 , 0x60},
	{0x03 , 0x02},
	{0x04 , 0x48},
	{0x05 , 0x18},
	{0x06 , 0x50},
	{0x07 , 0x10},
	{0x08 , 0x38},
	{0x0a , 0x80}, 
	{0x21 , 0x04},
	{0xfe , 0x00},
	{0x20 , 0x03},

	//// mipi
	{0xfe , 0x03},
	{0x12 , 0x40},
	{0x13 , 0x06},
	{0x04 , 0x01},
	{0x05 , 0x00},
	{0xfe , 0x00},
};
#endif
/*
 * The default register settings
 *
 */

static struct reg_value sensor_default_regs[] = {
	{0xfe, 0xf0},
	{0xfe, 0xf0},
	{0xfe, 0xf0},
	{0xfc, 0x06},
	{0xf6, 0x00},
	{0xf7, 0x1d},
	{0xf8, 0x84},
	{0xfa, 0x00},
	{0xf9, 0x8e},
	{0xf2, 0x00},
	/////////////////////////////////////////////////
	//////////////////ISP reg//////////////////////
	////////////////////////////////////////////////////
	{0xfe , 0x00},
	{0x03 , 0x04},
	{0x04 , 0xe2},
	{0x09 , 0x00},
	{0x0a , 0x00},
	{0x0b , 0x00},
	{0x0c , 0x00},
	{0x0d , 0x04},
	{0x0e , 0xc0},
	{0x0f , 0x06},
	{0x10 , 0x52},
	{0x12 , 0x2e},
	{0x17 , 0x14}, //mirror
	{0x18 , 0x22},
	{0x19 , 0x0e},
	{0x1a , 0x01},
	{0x1b , 0x4b},
	{0x1c , 0x07},
	{0x1d , 0x10},
	{0x1e , 0x88},
	{0x1f , 0x78},
	{0x20 , 0x03},
	{0x21 , 0x40},
	{0x22 , 0xa0}, 
	{0x24 , 0x16},
	{0x25 , 0x01},
	{0x26 , 0x10},
	{0x2d , 0x60},
	{0x30 , 0x01},
	{0x31 , 0x90},
	{0x33 , 0x06},
	{0x34 , 0x01},
	/////////////////////////////////////////////////
	//////////////////ISP reg////////////////////
	/////////////////////////////////////////////////
	{0xfe , 0x00},
	{0x80 , 0x7f},
	{0x81 , 0x26},
	{0x82 , 0xfa},
	{0x83 , 0x00},
	{0x84 , 0x03}, 
	{0x86 , 0x02},
	{0x88 , 0x03},
	{0x89 , 0x03},
	{0x85 , 0x08}, 
	{0x8a , 0x00},
	{0x8b , 0x00},
	{0xb0 , 0x55},
	{0xc3 , 0x00},
	{0xc4 , 0x80},
	{0xc5 , 0x90},
	{0xc6 , 0x3b},
	{0xc7 , 0x46},
	{0xec , 0x06},
	{0xed , 0x04},
	{0xee , 0x60},
	{0xef , 0x90},
	{0xb6 , 0x01},
	{0x90 , 0x01},
	{0x91 , 0x00},
	{0x92 , 0x00},
	{0x93 , 0x00},
	{0x94 , 0x00},
	{0x95 , 0x04},
	{0x96 , 0xb0},
	{0x97 , 0x06},
	{0x98 , 0x40},
	/////////////////////////////////////////
	/////////// BLK ////////////////////////
	/////////////////////////////////////////
	{0xfe , 0x00},
	{0x40 , 0x42},
	{0x41 , 0x00},
	{0x43 , 0x5b}, 
	{0x5e , 0x00}, 
	{0x5f , 0x00},
	{0x60 , 0x00}, 
	{0x61 , 0x00}, 
	{0x62 , 0x00},
	{0x63 , 0x00}, 
	{0x64 , 0x00}, 
	{0x65 , 0x00}, 
	{0x66 , 0x20},
	{0x67 , 0x20}, 
	{0x68 , 0x20}, 
	{0x69 , 0x20}, 
	{0x76 , 0x00},                                  
	{0x6a , 0x08}, 
	{0x6b , 0x08}, 
	{0x6c , 0x08}, 
	{0x6d , 0x08}, 
	{0x6e , 0x08}, 
	{0x6f , 0x08}, 
	{0x70 , 0x08}, 
	{0x71 , 0x08},   
	{0x76 , 0x00},
	{0x72 , 0xf0},
	{0x7e , 0x3c},
	{0x7f , 0x00},
	{0xfe , 0x02},
	{0x48 , 0x15},
	{0x49 , 0x00},
	{0x4b , 0x0b},
	{0xfe , 0x00},
	////////////////////////////////////////
	/////////// AEC ////////////////////////
	////////////////////////////////////////
	{0xfe , 0x01},
	{0x01 , 0x04},
	{0x02 , 0xc0},
	{0x03 , 0x04},
	{0x04 , 0x90},
	{0x05 , 0x30},
	{0x06 , 0x90},
	{0x07 , 0x30},
	{0x08 , 0x80},
	{0x09 , 0x00},
	{0x0a , 0x82},
	{0x0b , 0x11},
	{0x0c , 0x10},
	{0x11 , 0x10},
	{0x13 , 0x7b},
	{0x17 , 0x00},
	{0x1c , 0x11},
	{0x1e , 0x61},
	{0x1f , 0x35},
	{0x20 , 0x40},
	{0x22 , 0x40},
	{0x23 , 0x20},
	{0xfe , 0x02},
	{0x0f , 0x04},
	{0xfe , 0x01},
	{0x12 , 0x35},
	{0x15 , 0xb0},
	{0x10 , 0x31},
	{0x3e , 0x28},
	{0x3f , 0xb0},
	{0x40 , 0x90},
	{0x41 , 0x0f},
	
	/////////////////////////////
	//////// INTPEE /////////////
	/////////////////////////////
	{0xfe , 0x02},
	{0x90 , 0x6c},
	{0x91 , 0x03},
	{0x92 , 0xcb},
	{0x94 , 0x33},
	{0x95 , 0x84},
	{0x97 , 0x65},
	{0xa2 , 0x11},
	{0xfe , 0x00},
	/////////////////////////////
	//////// DNDD///////////////
	/////////////////////////////
	{0xfe , 0x02},
	{0x80 , 0xc1},
	{0x81 , 0x08},
	{0x82 , 0x05},
	{0x83 , 0x08},
	{0x84 , 0x0a},
	{0x86 , 0xf0},
	{0x87 , 0x50},
	{0x88 , 0x15},
	{0x89 , 0xb0},
	{0x8a , 0x30},
	{0x8b , 0x10},
	/////////////////////////////////////////
	/////////// ASDE ////////////////////////
	/////////////////////////////////////////
	{0xfe , 0x01},
	{0x21 , 0x04},
	{0xfe , 0x02},
	{0xa3 , 0x50},
	{0xa4 , 0x20},
	{0xa5 , 0x40},
	{0xa6 , 0x80},
	{0xab , 0x40},
	{0xae , 0x0c},
	{0xb3 , 0x46},
	{0xb4 , 0x64},
	{0xb6 , 0x38},
	{0xb7 , 0x01},
	{0xb9 , 0x2b},
	{0x3c , 0x04},
	{0x3d , 0x15},
	{0x4b , 0x06},
	{0x4c , 0x20},
	{0xfe , 0x00},
	/////////////////////////////////////////
	/////////// GAMMA   ////////////////////////
	/////////////////////////////////////////
	
	///////////////////gamma1////////////////////
	#if 1
	{0xfe , 0x02},
	{0x10 , 0x09},
	{0x11 , 0x0d},
	{0x12 , 0x13},
	{0x13 , 0x19},
	{0x14 , 0x27},
	{0x15 , 0x37},
	{0x16 , 0x45},
	{0x17 , 0x53},
	{0x18 , 0x69},
	{0x19 , 0x7d},
	{0x1a , 0x8f},
	{0x1b , 0x9d},
	{0x1c , 0xa9},
	{0x1d , 0xbd},
	{0x1e , 0xcd},
	{0x1f , 0xd9},
	{0x20 , 0xe3},
	{0x21 , 0xea},
	{0x22 , 0xef},
	{0x23 , 0xf5},
	{0x24 , 0xf9},
	{0x25 , 0xff},
	#else                               
	{0xfe , 0x02},
	{0x10 , 0x0a},
	{0x11 , 0x12},
	{0x12 , 0x19},
	{0x13 , 0x1f},
	{0x14 , 0x2c},
	{0x15 , 0x38},
	{0x16 , 0x42},
	{0x17 , 0x4e},
	{0x18 , 0x63},
	{0x19 , 0x76},
	{0x1a , 0x87},
	{0x1b , 0x96},
	{0x1c , 0xa2},
	{0x1d , 0xb8},
	{0x1e , 0xcb},
	{0x1f , 0xd8},
	{0x20 , 0xe2},
	{0x21 , 0xe9},
	{0x22 , 0xf0},
	{0x23 , 0xf8},
	{0x24 , 0xfd},
	{0x25 , 0xff},
	{0xfe , 0x00},     
	#endif 
	{0xfe , 0x00},     
	{0xc6 , 0x20},
	{0xc7 , 0x2b},
	///////////////////gamma2////////////////////
	#if 1
	{0xfe , 0x02},
	{0x26 , 0x0f},
	{0x27 , 0x14},
	{0x28 , 0x19},
	{0x29 , 0x1e},
	{0x2a , 0x27},
	{0x2b , 0x33},
	{0x2c , 0x3b},
	{0x2d , 0x45},
	{0x2e , 0x59},
	{0x2f , 0x69},
	{0x30 , 0x7c},
	{0x31 , 0x89},
	{0x32 , 0x98},
	{0x33 , 0xae},
	{0x34 , 0xc0},
	{0x35 , 0xcf},
	{0x36 , 0xda},
	{0x37 , 0xe2},
	{0x38 , 0xe9},
	{0x39 , 0xf3},
	{0x3a , 0xf9},
	{0x3b , 0xff},
	#else
	////Gamma outdoor
	{0xfe , 0x02},
	{0x26 , 0x17},
	{0x27 , 0x18},
	{0x28 , 0x1c},
	{0x29 , 0x20},
	{0x2a , 0x28},
	{0x2b , 0x34},
	{0x2c , 0x40},
	{0x2d , 0x49},
	{0x2e , 0x5b},
	{0x2f , 0x6d},
	{0x30 , 0x7d},
	{0x31 , 0x89},
	{0x32 , 0x97},
	{0x33 , 0xac},
	{0x34 , 0xc0},
	{0x35 , 0xcf},
	{0x36 , 0xda},
	{0x37 , 0xe5},
	{0x38 , 0xec},
	{0x39 , 0xf8},
	{0x3a , 0xfd},
	{0x3b , 0xff},
	#endif
	/////////////////////////////////////////////// 
	///////////YCP /////////////////////// 
	/////////////////////////////////////////////// 
	{0xfe , 0x02},
	{0xd1 , 0x32},
	{0xd2 , 0x32},
	{0xd3 , 0x40},
	{0xd6 , 0xf0},
	{0xd7 , 0x10},
	{0xd8 , 0xda},
	{0xdd , 0x14},
	{0xde , 0x86},
	{0xed , 0x80},
	{0xee , 0x00},
	{0xef , 0x3f},
	{0xd8 , 0xd8},
	///////////////////abs/////////////////
	{0xfe , 0x01},
	{0x9f , 0x40},
	/////////////////////////////////////////////
	//////////////////////// LSC ///////////////
	//////////////////////////////////////////
	{0xfe , 0x01},
	{0xc2 , 0x14},
	{0xc3 , 0x0d},
	{0xc4 , 0x0c},
	{0xc8 , 0x15},
	{0xc9 , 0x0d},
	{0xca , 0x0a},
	{0xbc , 0x24},
	{0xbd , 0x10},
	{0xbe , 0x0b},
	{0xb6 , 0x25},
	{0xb7 , 0x16},
	{0xb8 , 0x15},
	{0xc5 , 0x00},
	{0xc6 , 0x00},
	{0xc7 , 0x00},
	{0xcb , 0x00},
	{0xcc , 0x00},
	{0xcd , 0x00},
	{0xbf , 0x07},
	{0xc0 , 0x00},
	{0xc1 , 0x00},
	{0xb9 , 0x00},
	{0xba , 0x00},
	{0xbb , 0x00},
	{0xaa , 0x01},
	{0xab , 0x01},
	{0xac , 0x00},
	{0xad , 0x05},
	{0xae , 0x06},
	{0xaf , 0x0e},
	{0xb0 , 0x0b},
	{0xb1 , 0x07},
	{0xb2 , 0x06},
	{0xb3 , 0x17},
	{0xb4 , 0x0e},
	{0xb5 , 0x0e},
	{0xd0 , 0x09},
	{0xd1 , 0x00},
	{0xd2 , 0x00},
	{0xd6 , 0x08},
	{0xd7 , 0x00},
	{0xd8 , 0x00},
	{0xd9 , 0x00},
	{0xda , 0x00},
	{0xdb , 0x00},
	{0xd3 , 0x0a},
	{0xd4 , 0x00},
	{0xd5 , 0x00},
	{0xa4 , 0x00},
	{0xa5 , 0x00},
	{0xa6 , 0x77},
	{0xa7 , 0x77},
	{0xa8 , 0x77},
	{0xa9 , 0x77},
	{0xa1 , 0x80},
	{0xa2 , 0x80},
	               
	{0xfe , 0x01},
	{0xdf , 0x0d},
	{0xdc , 0x25},
	{0xdd , 0x30},
	{0xe0 , 0x77},
	{0xe1 , 0x80},
	{0xe2 , 0x77},
	{0xe3 , 0x90},
	{0xe6 , 0x90},
	{0xe7 , 0xa0},
	{0xe8 , 0x90},
	{0xe9 , 0xa0},                                      
	{0xfe , 0x00},
	///////////////////////////////////////////////
	/////////// AWB////////////////////////
	///////////////////////////////////////////////
	{0xfe , 0x01},
	{0x4f , 0x00},
	{0x4f , 0x00},
	{0x4b , 0x01},
	{0x4f , 0x00},
	         
	{0x4c , 0x01}, // D75
	{0x4d , 0x71},
	{0x4e , 0x01},
	{0x4c , 0x01},
	{0x4d , 0x91},
	{0x4e , 0x01},
	{0x4c , 0x01},
	{0x4d , 0x70},
	{0x4e , 0x01},
	         
	{0x4c , 0x01}, // D65
	{0x4d , 0x90},
	{0x4e , 0x02},                                    
	         
	         
	{0x4c , 0x01},
	{0x4d , 0xb0},
	{0x4e , 0x02},
	{0x4c , 0x01},
	{0x4d , 0x8f},
	{0x4e , 0x02},
	{0x4c , 0x01},
	{0x4d , 0x6f},
	{0x4e , 0x02},
	{0x4c , 0x01},
	{0x4d , 0xaf},
	{0x4e , 0x02},
	         
	{0x4c , 0x01},
	{0x4d , 0xd0},
	{0x4e , 0x02},
	{0x4c , 0x01},
	{0x4d , 0xf0},
	{0x4e , 0x02},
	{0x4c , 0x01},
	{0x4d , 0xcf},
	{0x4e , 0x02},
	{0x4c , 0x01},
	{0x4d , 0xef},
	{0x4e , 0x02},
	         
	{0x4c , 0x01},//D50
	{0x4d , 0x6e},
	{0x4e , 0x03},
	{0x4c , 0x01}, 
	{0x4d , 0x8e},
	{0x4e , 0x03},
	{0x4c , 0x01},
	{0x4d , 0xae},
	{0x4e , 0x03},
	{0x4c , 0x01},
	{0x4d , 0xce},
	{0x4e , 0x03},
	{0x4c , 0x01},
	{0x4d , 0x4d},
	{0x4e , 0x03},
	{0x4c , 0x01},
	{0x4d , 0x6d},
	{0x4e , 0x03},
	{0x4c , 0x01},
	{0x4d , 0x8d},
	{0x4e , 0x03},
	{0x4c , 0x01},
	{0x4d , 0xad},
	{0x4e , 0x03},
	{0x4c , 0x01},
	{0x4d , 0xcd},
	{0x4e , 0x03},
	{0x4c , 0x01},
	{0x4d , 0x4c},
	{0x4e , 0x03},
	{0x4c , 0x01},
	{0x4d , 0x6c},
	{0x4e , 0x03},
	{0x4c , 0x01},
	{0x4d , 0x8c},
	{0x4e , 0x03},
	{0x4c , 0x01},
	{0x4d , 0xac},
	{0x4e , 0x03},
	{0x4c , 0x01},
	{0x4d , 0xcc},
	{0x4e , 0x03},
	         
	{0x4c , 0x01},
	{0x4d , 0xcb},
	{0x4e , 0x03},
	         
	{0x4c , 0x01},
	{0x4d , 0x4b},
	{0x4e , 0x03},
	{0x4c , 0x01},
	{0x4d , 0x6b},
	{0x4e , 0x03},
	{0x4c , 0x01},
	{0x4d , 0x8b},
	{0x4e , 0x03},
	{0x4c , 0x01},
	{0x4d , 0xab},
	{0x4e , 0x03},
	         
	{0x4c , 0x01},//CWF
	{0x4d , 0x8a},
	{0x4e , 0x04},
	{0x4c , 0x01},
	{0x4d , 0xaa},
	{0x4e , 0x04},
	{0x4c , 0x01},
	{0x4d , 0xca},
	{0x4e , 0x04},
	{0x4c , 0x01},
	{0x4d , 0xca},
	{0x4e , 0x04},
	{0x4c , 0x01},
	{0x4d , 0xc9},
	{0x4e , 0x04},
	{0x4c , 0x01},
	{0x4d , 0x8a},
	{0x4e , 0x04},
	{0x4c , 0x01},
	{0x4d , 0x89},
	{0x4e , 0x04},
	{0x4c , 0x01},
	{0x4d , 0xa9},
	{0x4e , 0x04},
	         
	         
	         
	{0x4c , 0x02},//tl84
	{0x4d , 0x0b},
	{0x4e , 0x05},
	{0x4c , 0x02},
	{0x4d , 0x0a},
	{0x4e , 0x05},
	         
	{0x4c , 0x01},
	{0x4d , 0xeb},
	{0x4e , 0x05},
	         
	{0x4c , 0x01},
	{0x4d , 0xea},
	{0x4e , 0x05},
	                 
	{0x4c , 0x02},
	{0x4d , 0x09},
	{0x4e , 0x05},
	{0x4c , 0x02},
	{0x4d , 0x29},
	{0x4e , 0x05},
	                     
	{0x4c , 0x02},
	{0x4d , 0x2a},
	{0x4e , 0x05},
	                      
	{0x4c , 0x02},
	{0x4d , 0x4a},
	{0x4e , 0x05},
	
	//{0x4c , 0x02}, //A
	//{0x4d , 0x6a},
	//{0x4e , 0x06},
	
	{0x4c , 0x02}, 
	{0x4d , 0x8a},
	{0x4e , 0x06},
	                
	{0x4c , 0x02},
	{0x4d , 0x49},
	{0x4e , 0x06},
	{0x4c , 0x02},
	{0x4d , 0x69},
	{0x4e , 0x06},
	{0x4c , 0x02},
	{0x4d , 0x89},
	{0x4e , 0x06},
	{0x4c , 0x02},
	{0x4d , 0xa9},
	{0x4e , 0x06},
	               
	{0x4c , 0x02},
	{0x4d , 0x48},
	{0x4e , 0x06},
	{0x4c , 0x02},
	{0x4d , 0x68},
	{0x4e , 0x06},
	{0x4c , 0x02},
	{0x4d , 0x69},
	{0x4e , 0x06},
	             
	{0x4c , 0x02},//H
	{0x4d , 0xca},
	{0x4e , 0x07},
	{0x4c , 0x02},
	{0x4d , 0xc9},
	{0x4e , 0x07},
	{0x4c , 0x02},
	{0x4d , 0xe9},
	{0x4e , 0x07},
	{0x4c , 0x03},
	{0x4d , 0x09},
	{0x4e , 0x07},
	{0x4c , 0x02},
	{0x4d , 0xc8},
	{0x4e , 0x07},
	{0x4c , 0x02},
	{0x4d , 0xe8},
	{0x4e , 0x07},
	{0x4c , 0x02},
	{0x4d , 0xa7},
	{0x4e , 0x07},
	{0x4c , 0x02},
	{0x4d , 0xc7},
	{0x4e , 0x07},
	{0x4c , 0x02},
	{0x4d , 0xe7},
	{0x4e , 0x07},
	{0x4c , 0x03},
	{0x4d , 0x07},
	{0x4e , 0x07},
	
	{0x4f , 0x01},
	{0x50 , 0x80},
	{0x51 , 0xa8},
	{0x52 , 0x47},
	{0x53 , 0x38},
	{0x54 , 0xc7},
	{0x56 , 0x0e},
	{0x58 , 0x08},
	{0x5b , 0x00},
	{0x5c , 0x74},
	{0x5d , 0x8b},
	{0x61 , 0xdb},
	{0x62 , 0xb8},
	{0x63 , 0x86},
	{0x64 , 0xc0},
	{0x65 , 0x04},
	
	{0x67 , 0xa8},
	{0x68 , 0xb0},
	{0x69 , 0x00},
	{0x6a , 0xa8},
	{0x6b , 0xb0},
	{0x6c , 0xaf},
	{0x6d , 0x8b},
	{0x6e , 0x50},
	{0x6f , 0x18},
	{0x73 , 0xf0},
	{0x70 , 0x0d},
	{0x71 , 0x60},
	{0x72 , 0x80},
	{0x74 , 0x01},
	{0x75 , 0x01},
	{0x7f , 0x0c},
	{0x76 , 0x70},
	{0x77 , 0x58},
	{0x78 , 0xa0},
	{0x79 , 0x5e},
	{0x7a , 0x54},
	{0x7b , 0x58},                                      
	{0xfe , 0x00},
	//////////////////////////////////////////
	///////////CC////////////////////////
	//////////////////////////////////////////
	{0xfe , 0x02},
	{0xc0 , 0x01},                                   
	{0xc1 , 0x44},
	{0xc2 , 0xfd},
	{0xc3 , 0x04},
	{0xc4 , 0xf0},
	{0xc5 , 0x48},
	{0xc6 , 0xfd},
	{0xc7 , 0x46},
	{0xc8 , 0xfd},
	{0xc9 , 0x02},
	{0xca , 0xe0},
	{0xcb , 0x45},
	{0xcc , 0xec},                         
	{0xcd , 0x48},
	{0xce , 0xf0},
	{0xcf , 0xf0},
	{0xe3 , 0x0c},
	{0xe4 , 0x4b},
	{0xe5 , 0xe0},
	//////////////////////////////////////////
	///////////ABS ////////////////////
	//////////////////////////////////////////
	{0xfe , 0x01},
	{0x9f , 0x40},
	{0xfe , 0x00}, 
	//////////////////////////////////////
	///////////  OUTPUT   ////////////////
	//////////////////////////////////////
	{0xfe, 0x00},
	{0xf2, 0x00},
	
	//////////////frame rate 50Hz/////////
	{0xfe , 0x00},
	{0x05 , 0x01},
	{0x06 , 0x56},
	{0x07 , 0x00},
	{0x08 , 0x32},
	{0xfe , 0x01},
	{0x25 , 0x00},
	{0x26 , 0xfa}, 
	{0x27 , 0x04}, 
	{0x28 , 0xe2}, //20fps 
	{0x29 , 0x06}, 
	{0x2a , 0xd6}, //14fps 
	{0x2b , 0x07}, 
	{0x2c , 0xd0}, //12fps
	{0x2d , 0x0b}, 
	{0x2e , 0xb8}, //8fps
	{0xfe , 0x00},
	
	///////////////dark sun////////////////////
	{0x18 , 0x22}, 
	{0xfe , 0x02},
	{0x40 , 0xbf},
	{0x46 , 0xcf},
	{0xfe , 0x00},
	
	/////////////////////////////////////////////////////
	//////////////////////   MIPI   /////////////////////
	/////////////////////////////////////////////////////
	{0xfe, 0x03},
	{0x02, 0x22},
	{0x03, 0x10}, // 0x12 20140821
	{0x04, 0x10}, // 0x01 
	{0x05, 0x00},
	{0x06, 0x88},
	
	{0x01, 0x83},
	{0x10, 0x84},
	
	{0x11, 0x1e},
	{0x12, 0x80},
	{0x13, 0x0c},
	{0x15, 0x10},
	{0x17, 0xf0},
	
	{0x21, 0x10},
	{0x22, 0x04},
	{0x23, 0x10},
	{0x24, 0x10},
	{0x25, 0x10},
	{0x26, 0x05},
	{0x29, 0x03},
	{0x2a, 0x0a},
	{0x2b, 0x06},
	{0xfe, 0x00},	
};
static const struct reg_value gc2145_setting_30fps_QUXGA_800_600[] = {
		//SENSORDB("GC2145_Sensor_SVGA"},
	{0xfe, 0x00},
	{0xb6, 0x01},
	{0xfd, 0x01},
	{0xfa, 0x00},
		//// crop window
	{0xfe, 0x00},
	{0x90, 0x01},
	{0x91, 0x00},
	{0x92, 0x00},
	{0x93, 0x00},
	{0x94, 0x00},
	{0x95, 0x02},
	{0x96, 0x58},
	{0x97, 0x03},
	{0x98, 0x20},
	{0x99, 0x11},
	{0x9a, 0x06},
		//// AWB
	{0xfe, 0x00},
	{0xec, 0x02},
	{0xed, 0x02},
	{0xee, 0x30},
	{0xef, 0x48},
	{0xfe, 0x02},
	{0x9d, 0x08},
	{0xfe, 0x01},
	{0x74, 0x00},
		//// AEC
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0x60},
	{0x03, 0x02},
	{0x04, 0x48},
	{0x05, 0x18},
	{0x06, 0x50},
	{0x07, 0x10},
	{0x08, 0x38},
	{0x0a, 0x80},
	{0x21, 0x04},
	{0xfe, 0x00},
	{0x20, 0x03},
	{0xfe, 0x00},
};
static const struct reg_value gc2145_setting_30fps_720P_1280_720[] = {
	{0xfe , 0x00},
//	{0xb6 , 0x01},
	{0xfd , 0x00},
	{0xfa , 0x11},
	//// crop window
	{0xfe , 0x00},

	{0x90 , 0x01},
	{0x91 , 0x00},
	{0x92 , 0x00},
	{0x93 , 0x00},
	{0x94 , 0x00},
	{0x95 , 0x02},
	{0x96 , 0xd0},
	{0x97 , 0x05},
	{0x98 , 0x00},
	{0x99 , 0x11},  
	{0x9a , 0x06},

		//// AWB
	{0xfe, 0x00},
	{0xec, 0x06},
	{0xed, 0x04},
	{0xee, 0x60},
	{0xef, 0x90},
	{0xfe, 0x02},
	{0x9d, 0x08},
	{0xfe, 0x01},
	{0x74, 0x01},
		//// AEC
	{0xfe, 0x01},
	{0x01, 0x04},
	{0x02, 0xc0},
	{0x03, 0x04},
	{0x04, 0x90},
	{0x05, 0x30},
	{0x06, 0x90},
	{0x07, 0x30},
	{0x08, 0x80},
	{0x0a, 0x82},
	{0xfe, 0x01},
	{0x21, 0x15},
	{0xfe, 0x00},
	{0x20, 0x15},
	{0xfe, 0x00},
	//// mipi
	{0xfe , 0x03},
	{0x12 , 0x00},
	{0x13 , 0x0A},
	{0x04 , 0x01},
	{0x05 , 0x00},
	{0xfe , 0x00},

};

static const struct reg_value gc2145_setting_30fps_UXGA_1600_1200[] = {
		//SENSORDB("GC2145_Sensor_2M"},
	{0xfe , 0x00},
	{0xfd , 0x00},
	{0xfa , 0x11},
		//// crop window
	{0xfe , 0x00},
	{0x90 , 0x01},
	{0x91 , 0x00},
	{0x92 , 0x00},
	{0x93 , 0x00},
	{0x94 , 0x00},
	{0x95 , 0x04},
	{0x96 , 0xb0},
	{0x97 , 0x06},
	{0x98 , 0x40},
	{0x99 , 0x11},
	{0x9a , 0x06},
		//// AWB
	{0xfe , 0x00},
	{0xec , 0x06},
	{0xed , 0x04},
	{0xee , 0x60},
	{0xef , 0x90},
	{0xfe , 0x01},
	{0x74 , 0x01},
		//// AEC
	{0xfe , 0x01},
	{0x01 , 0x04},
	{0x02 , 0xc0},
	{0x03 , 0x04},
	{0x04 , 0x90},
	{0x05 , 0x30},
	{0x06 , 0x90},
	{0x07 , 0x30},
	{0x08 , 0x80},
	{0x0a , 0x82},
	{0xfe , 0x01},
	{0x21 , 0x15},
	{0xfe , 0x00},
	{0x20 , 0x15},//if 0xfa=11,then 0x21=15;else if 0xfa=00,then 0x21=04
	//// mipi
	{0xfe , 0x03},
	{0x12 , 0x80},
	{0x13 , 0x0c},
	{0x04 , 0x01},
	{0x05 , 0x00},
	{0xfe , 0x00},
};
static const struct gc2145_mode_info gc2145_mode_init_data = {
#if 0
	"mode_quxga_800_600", gc2145_MODE_QUXGA_800_600, 1280, 720,
	gc2145_svga_settings,
	ARRAY_SIZE(gc2145_svga_settings),

#else
	"mode_720p_1280_720", gc2145_MODE_720P_1280_720,
	 1280, 720, gc2145_setting_30fps_720P_1280_720,
	 ARRAY_SIZE(gc2145_setting_30fps_720P_1280_720),
#endif
};

static const struct gc2145_mode_info gc2145_mode_data[gc2145_MODE_MAX] = {
//	{"mode_quxga_800_600", gc2145_MODE_QUXGA_800_600,
//	 1280, 720, gc2145_svga_settings,
//	 ARRAY_SIZE(gc2145_svga_settings)},
	{"mode_720p_1280_720", gc2145_MODE_720P_1280_720,
	 1280, 720, gc2145_setting_30fps_720P_1280_720,
	 ARRAY_SIZE(gc2145_setting_30fps_720P_1280_720)},
//	{"mode_uxga_1600_1200", gc2145_MODE_UXGA_1600_1200,
//	 1600, 1200, gc2145_setting_30fps_UXGA_1600_1200,
//	 ARRAY_SIZE(gc2145_setting_30fps_UXGA_1600_1200)},
};

static struct gc2145_dev *to_gc2145_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct gc2145_dev, sd);
}

static struct device *gc2145_to_dev(struct gc2145_dev *sensor)
{
	return &sensor->i2c_client->dev;
}

static inline struct v4l2_subdev *ctrl_to_sd(struct v4l2_ctrl *ctrl)
{
	return &container_of(ctrl->handler, struct gc2145_dev,
			     ctrls.handler)->sd;
}

static int __gc2145_write_reg(struct gc2145_dev *sensor, u8 reg,
			      unsigned int len, u8 val)
{
	struct i2c_client *client = sensor->i2c_client;
	u8 buf[2];
	int ret;
	struct i2c_msg msg[2];
#if 0
	if (len > 4)
		return -EINVAL;

	//put_unaligned_be16(reg, buf);
	//put_unaligned_be32(val << (8 * (4 - len)), buf + 2);
	
	buf[0] = reg;
	buf[1] = val;

	ret = i2c_master_send(client, buf, len + 1);
	if (ret != len + 1) {
		dev_err(&client->dev, "write error: reg=0x%02x: val=0x%02x %d\n", reg,val, ret);
		return -EIO;
	}
#else
	buf[0] = reg;
	buf[1] = val;

        msg[0].addr = client->addr;
        msg[0].flags = 0;
        msg[0].buf= buf;
        msg[0].len = len+1;

        ret = i2c_transfer(client->adapter, &msg[0], 1);

	if(ret < 0){
		dev_err(&client->dev, "GLS write error: reg=0x%02x: value:0x%02x ret=%d\n", reg,val, ret);
		return -EIO;
	}
#endif
	return 0;
}

#define gc2145_write_reg(s, r, v) \
	__gc2145_write_reg(s, r, gc2145_REG_VALUE_8BIT, v)

static int __gc2145_read_reg(struct gc2145_dev *sensor, u8 reg,
			     unsigned int len, u8 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msgs[2];
	//u8 addr_buf;//[2];// = { reg >> 8, reg & 0xff };
//	u8 data_buf[4] = { 0, };
	int ret;

	if (len > 4)
		return -EINVAL;



	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;//ARRAY_SIZE(addr_buf);
	msgs[0].buf = &reg;//addr_buf;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;//len;
	msgs[1].buf = val;//&data_buf[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs)) {
		dev_err(&client->dev, "GLS read error: reg=0x%2x: %d\n", reg, ret);
		return -EIO;
	}

	//*val = get_unaligned_be32(data_buf);

	return 0;
}

#define gc2145_read_reg(s, r, v) \
	__gc2145_read_reg(s, r, gc2145_REG_VALUE_8BIT, v)

static int sensor_write_array(struct gc2145_dev *sensor , struct reg_value *regs, int array_size)
{
	u8 reg_addr;
	u8 val;
	int ret = 0;
	int i;
	GLS("%s >>> start ... array_size = %d \n", __func__, array_size);
	for (i = 0; i < array_size; ++i, ++regs) {
		reg_addr = regs->reg_addr;
		val = regs->val;

		ret = gc2145_write_reg(sensor, reg_addr, val);
		if (ret){
			printk(" %s err.i=%d addr(0x%x) val(0x%x)\n", __func__,i, reg_addr, val);
			return -1;
		}
	}
	GLS("%s <<< end ... array_size = %d \n", __func__, array_size);
	return 0;
}


#if 0 //not use
static int gc2145_mod_reg(struct gc2145_dev *sensor, u16 reg, u8 mask, u8 val)
{
	u8 readval;
	int ret;

	ret = gc2145_read_reg(sensor, reg, &readval);
	if (ret < 0)
		return ret;

	readval &= ~mask;
	val &= mask;
	val |= readval;

	return gc2145_write_reg(sensor, reg, val);
}
#endif
static int gc2145_load_regs(struct gc2145_dev *sensor,
			    const struct gc2145_mode_info *mode)
{
	const struct reg_value *regs = mode->reg_data;
	unsigned int i;
	int ret = 0;
	u16 reg_addr;
	u8 val;
//	unsigned  int  temp=0,shutter=0;
GLS("%s w:%d h:%d \n",__func__, mode->width, mode->height);
#if 0
	if((mode->width==1600)&&(mode->height==1200))  //capture mode  >640*480
	{

		gc2145_write_reg(sensor, 0xfe, 0x00);
		gc2145_write_reg(sensor, 0xb6, 0x00);

		/*read shutter */
		gc2145_read_reg(sensor, 0x03, &val);
		temp |= (val<< 8);			  
		gc2145_read_reg(sensor, 0x04, &val);
		temp |= (val & 0xff);		  
		shutter=temp;
	}
#endif
	for (i = 0; i < mode->reg_data_size; ++i, ++regs) {
		reg_addr = regs->reg_addr;
		val = regs->val;

		ret = gc2145_write_reg(sensor, reg_addr, val);
		if (ret)
			break;
	}
#if 0
	if((mode->width==1600)&&(mode->height==1200))  //capture mode  >640*480
	{

		gc2145_write_reg(sensor, 0xfe, 0x00);
		shutter= shutter /2;
		if(shutter < 1) shutter = 1;
		val = ((shutter>>8)&0xff);
		gc2145_write_reg(sensor, 0x03, val);
		val = (shutter&0xff);
		gc2145_write_reg(sensor, 0x04, val);
		msleep(200);

	}
#endif

	return ret;
}

static void gc2145_power_up(struct gc2145_dev *sensor)
{
GLS("%s >>> \n",__func__);
//	if (sensor->reset_gpio < 0 || sensor->powdn_gpio < 0 ||
//			sensor->avdd_gpio < 0 || sensor->dovdd_gpio < 0)
//		return;

//	gpio_direction_output(sensor->avdd_gpio, 1);
//	usleep_range(5000, 10000);

//	gpio_direction_output(sensor->dovdd_gpio, 1);
//	usleep_range(5000, 10000);
	
	gpio_direction_output(sensor->powdn_gpio, 0); //add by soar at 20210906
	usleep_range(5000, 10000);

	//mdelay(2000);
	
	gpio_direction_output(sensor->reset_gpio, 1);
	usleep_range(5000, 10000);

	//mdelay(2000);
GLS("%s <<< \n",__func__);
}

static void gc2145_power_down(struct gc2145_dev *sensor)
{
GLS("%s >>> \n",__func__);
//	if (sensor->reset_gpio < 0 || sensor->powdn_gpio < 0 ||
//			sensor->avdd_gpio < 0 || sensor->dovdd_gpio < 0)
//		return;

//	gpio_direction_output(sensor->avdd_gpio, 0);
//	usleep_range(5000, 10000);

//	gpio_direction_output(sensor->dovdd_gpio, 0);
//	usleep_range(5000, 10000);

	gpio_direction_output(sensor->reset_gpio, 0);
	usleep_range(5000, 10000);
	//mdelay(2000);
	
	gpio_direction_output(sensor->powdn_gpio, 1); //add by soar at 20210906
	usleep_range(5000, 10000);
	//mdelay(2000);
GLS("%s <<< \n",__func__);
}

static int gc2145_mode_set(struct gc2145_dev *sensor)
{
	//struct gc2145_ctrls *ctrls = &sensor->ctrls;
	int ret;

GLS("%s >>> \n",__func__);

	ret = gc2145_load_regs(sensor, sensor->current_mode);
	if (ret < 0)
		return ret;


	sensor->mode_pending_changes = false;
GLS("%s <<< \n",__func__);

	return 0;
}

static int gc2145_mode_restore(struct gc2145_dev *sensor)
{
	int ret;

GLS("%s >>> \n",__func__);
	//ret = gc2145_load_regs(sensor, &gc2145_mode_init_data);
	ret = gc2145_load_regs(sensor, sensor->current_mode );
	if (ret < 0){
		printk(" gc2145_mode_restore write err \n");
		return ret;
	}

GLS("%s <<< \n",__func__);
	return gc2145_mode_set(sensor);
}

static int gc2145_power_off(struct gc2145_dev *sensor)
{
GLS("%s >>> \n",__func__);
	//if (!sensor->is_enabled)
	//	return 0;

	clk_disable_unprepare(sensor->xclk);  //fix soar at 20210906
	gc2145_power_down(sensor);
	//regulator_bulk_disable(gc2145_NUM_SUPPLIES, sensor->supplies);
	//sensor->is_enabled = false;

GLS("%s <<< \n",__func__);
	return 0;
}

static int gc2145_power_on(struct gc2145_dev *sensor)
{
	//struct device *dev = gc2145_to_dev(sensor);
	int ret;

GLS("%s >>> \n",__func__);
	//if (sensor->is_enabled)
	//	return 0;

	/*ret = regulator_bulk_enable(gc2145_NUM_SUPPLIES, sensor->supplies);
	if (ret < 0) {
		dev_err(dev, "failed to enable regulators: %d\n", ret);
		return ret;
	} */


	ret = clk_prepare_enable(sensor->xclk);   //fix soar at 20210906
	if (ret < 0){
		printk("gc2415 open clk err\n");
		return ret;
	}

	//gc2145_power_down(sensor);
	gc2145_power_up(sensor);

	//sensor->is_enabled = true;

	/* Set clock lane into LP-11 state */
//	gc2145_stream_enable(sensor);
	//usleep_range(1000, 2000);
//	gc2145_stream_disable(sensor);

GLS("%s <<< \n",__func__);
	return 0;
}

static int gc2145_s_power(struct v4l2_subdev *sd, int on)
{
	struct gc2145_dev *sensor = to_gc2145_dev(sd);
	int ret = 0;

GLS("%s >>>  on = %d \n",__func__, on);
	mutex_lock(&sensor->lock);
	if (on)
		ret = gc2145_power_on(sensor);
	else
		ret = gc2145_power_off(sensor);

	mutex_unlock(&sensor->lock);

	if (on && ret == 0) {
	//	ret = v4l2_ctrl_handler_setup(&sensor->ctrls.handler);
	//	if (ret < 0)
	//		return ret;

		ret = sensor_write_array(sensor, sensor_default_regs , ARRAY_SIZE(sensor_default_regs));
		if (ret < 0){
			printk("gc2145 write default regs error !!!!\n");
			//goto lock_destroy;
			return ret;
		}
		//ret = gc2145_mode_restore(sensor);
	}

GLS("%s <<< \n",__func__);
	return ret;
}

static int gc2145_s_g_frame_interval(struct v4l2_subdev *sd,
				     struct v4l2_subdev_frame_interval *fi)
{
	struct gc2145_dev *sensor = to_gc2145_dev(sd);

GLS("%s >>> \n",__func__);
	mutex_lock(&sensor->lock);
	fi->interval = sensor->frame_interval;
	mutex_unlock(&sensor->lock);

GLS("%s <<< \n",__func__);
	return 0;
}

static int gc2145_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct gc2145_dev *sensor = to_gc2145_dev(sd);
	int ret = 0;

#if 0
GLS("%s >>> \n",__func__);
	mutex_lock(&sensor->lock);

	if (sensor->is_streaming == !!enable)
		goto unlock;

	if (enable && sensor->mode_pending_changes) {
		ret = gc2145_mode_set(sensor);
		if (ret < 0)
			goto unlock;
	}

	if (enable)
		ret = gc2145_stream_enable(sensor);
	else
		ret = gc2145_stream_disable(sensor);

	sensor->is_streaming = !!enable;

unlock:
	mutex_unlock(&sensor->lock);

GLS("%s <<< \n",__func__);
	return ret;
	#else
	GLS("%s    %s \n" , __func__, enable?"start":"stop");

	if(enable){
			ret = sensor_write_array(sensor, gc2145_start_settings, ARRAY_SIZE(gc2145_start_settings));
		if (ret < 0){
			printk("gc2145 write start settings error !!!!\n");
			//goto lock_destroy;
			return ret;
		}


	}else{
			ret = sensor_write_array(sensor, gc2145_stop_settings, ARRAY_SIZE(gc2145_stop_settings));
		if (ret < 0){
			printk("gc2145 write start settings error !!!!\n");
			//goto lock_destroy;
			return ret;
		}


	}

//John_gao
	return 0;
	#endif
}

static int gc2145_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct gc2145_dev *sensor = to_gc2145_dev(sd);

GLS("%s >>> \n",__func__);
	if (code->pad != 0 || code->index != 0)
		return -EINVAL;

	code->code = sensor->fmt.code;

GLS("%s <<< \n",__func__);
	return 0;
}

static int gc2145_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct gc2145_dev *sensor = to_gc2145_dev(sd);
	struct v4l2_mbus_framefmt *fmt = NULL;
	int ret = 0;
GLS("%s >>> \n",__func__);

	if (format->pad != 0)
		return -EINVAL;

	mutex_lock(&sensor->lock);

	if (format->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt = v4l2_subdev_get_try_format(&sensor->sd, cfg, format->pad);
#else
		ret = -EINVAL;
#endif
	} else {
		fmt = &sensor->fmt;
	}

	if (fmt)
		format->format = *fmt;

	mutex_unlock(&sensor->lock);

GLS("%s <<< \n",__func__);
	return ret;
}

static int gc2145_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct gc2145_dev *sensor = to_gc2145_dev(sd);
	struct v4l2_mbus_framefmt *fmt = &format->format;
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	struct v4l2_mbus_framefmt *try_fmt;
#endif
	const struct gc2145_mode_info *mode;
	int ret = 0;

GLS("%s >>> \n",__func__);
	if (format->pad != 0)
		return -EINVAL;

	mutex_lock(&sensor->lock);

	if (sensor->is_streaming) {
		ret = -EBUSY;
		goto unlock;
	}
printk("fmt->width(%d), fmt->height(%d)\n", fmt->width, fmt->height);
	mode = v4l2_find_nearest_size(gc2145_mode_data,
				      ARRAY_SIZE(gc2145_mode_data), width,
				      height, fmt->width, fmt->height);
	if (!mode) {
		ret = -EINVAL;
		goto unlock;
	}

	if (format->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		try_fmt = v4l2_subdev_get_try_format(sd, cfg, 0);
		format->format = *try_fmt;
#endif
		goto unlock;
	}

	fmt->width = mode->width;
	fmt->height = mode->height;
printk("fmt->width(%d), fmt->height(%d)\n", fmt->width, fmt->height);
	fmt->code = sensor->fmt.code;
	fmt->colorspace = sensor->fmt.colorspace;

	sensor->current_mode = mode;
	sensor->fmt = format->format;
	sensor->mode_pending_changes = true;

	//John_gao 
	gc2145_mode_restore(sensor);
#if 1
	{
		u8 val = -1;
		gc2145_read_reg(sensor, 0x84, &val);
		GLS(" yuv 0x84 = 0x%02x\n", val);
	}
	ret = sensor_write_array(sensor, sensor_fmt_yuv422_uyvy, ARRAY_SIZE(sensor_fmt_yuv422_uyvy));
		if (ret < 0){
			printk("gc2145 write yuv422 uyvy regs error !!!!\n");
			//goto lock_destroy;
			return ret;
		}
	{
		u8 val = -1;
		gc2145_read_reg(sensor, 0x84, &val);
		GLS(" yuv 0x84 = 0x%02x\n", val);
	}

//	ret = sensor_write_array(sensor, gc2145_svga_settings , ARRAY_SIZE(gc2145_svga_settings));
//		if (ret < 0){
//			printk("gc2145 write mipi regs error !!!!\n");
//			//goto lock_destroy;
//			return ret;
//		}
#endif

unlock:
	mutex_unlock(&sensor->lock);

GLS("%s <<< \n",__func__);
	return ret;
}

static int gc2145_init_cfg(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg)
{
	struct v4l2_subdev_format fmt = {
		.which = cfg ? V4L2_SUBDEV_FORMAT_TRY
				: V4L2_SUBDEV_FORMAT_ACTIVE,
		.format = {
			.width = 800,
			.height = 600,
		}
	};

	return gc2145_set_fmt(sd, cfg, &fmt);
}

static int gc2145_enum_frame_size(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_frame_size_enum *fse)
{
	int index = fse->index;

GLS("%s >>> \n",__func__);
	if (index >= gc2145_MODE_MAX || index < 0)
		return -EINVAL;

	fse->min_width = gc2145_mode_data[index].width;
	fse->min_height = gc2145_mode_data[index].height;
	fse->max_width = gc2145_mode_data[index].width;
	fse->max_height = gc2145_mode_data[index].height;

GLS("%s <<< \n",__func__);
	return 0;
}

static int gc2145_enum_frame_interval(struct v4l2_subdev *sd,
			      struct v4l2_subdev_pad_config *cfg,
			      struct v4l2_subdev_frame_interval_enum *fie)
{
	struct v4l2_fract tpf;

GLS("%s >>> \n",__func__);
	if (fie->index >= gc2145_MODE_MAX || fie->width > gc2145_WIDTH_MAX ||
	    fie->height > gc2145_HEIGHT_MAX ||
	    fie->which > V4L2_SUBDEV_FORMAT_ACTIVE)
		return -EINVAL;

	tpf.denominator = 15;//gc2145_FRAME_RATE;
	tpf.numerator = 1;

	fie->interval = tpf;
GLS("%s <<< \n",__func__);

	return 0;
}

static int gc2145_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
#if 0
	struct v4l2_subdev *sd = ctrl_to_sd(ctrl);
	struct gc2145_dev *sensor = to_gc2145_dev(sd);
	struct gc2145_ctrls *ctrls = &sensor->ctrls;
	int val;

GLS("%s >>> \n",__func__);
	if (!sensor->is_enabled)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		val = gc2145_gain_get(sensor);
		if (val < 0)
			return val;
		ctrls->gain->val = val;
		break;
	case V4L2_CID_EXPOSURE:
		val = gc2145_exposure_get(sensor);
		if (val < 0)
			return val;
		ctrls->exposure->val = val;
		break;
	}
GLS("%s <<< \n",__func__);
#endif
	return 0;
}

static int gc2145_s_ctrl(struct v4l2_ctrl *ctrl)
{
	//struct v4l2_subdev *sd = ctrl_to_sd(ctrl);
	//struct gc2145_dev *sensor = to_gc2145_dev(sd);
	//struct gc2145_ctrls *ctrls = &sensor->ctrls;

GLS("%s >>> \n",__func__);
//	if (!sensor->is_enabled)
	if(1)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_AUTOGAIN:
		GLS("V4L2_CID_AUTOGAIN\n");
		return 1;//gc2145_gain_set(sensor, !!ctrl->val);
	case V4L2_CID_GAIN:
		GLS("V4L2_CID_GAIN\n");
		return 1;//gc2145_gain_set(sensor, !!ctrls->auto_gain->val);
	case V4L2_CID_EXPOSURE_AUTO:
		GLS("V4L2_CID_EXPOSURE_AUTO\n");
		return 1;//gc2145_exposure_set(sensor, !!ctrl->val);
	case V4L2_CID_EXPOSURE:
		GLS("V4L2_CID_EXPOSURE\n");
		return 1;//gc2145_exposure_set(sensor, !!ctrls->auto_exp->val);
	case V4L2_CID_VFLIP:
		GLS("V4L2_CID_VFLIP\n");
		return 1 ; 
	case V4L2_CID_HFLIP:
		GLS("V4L2_CID_HFLIP\n");
	return 1 ; 
 

	default:
		break;
	}
GLS("%s <<< \n",__func__);

	return -EINVAL;
}

static const struct v4l2_ctrl_ops gc2145_ctrl_ops = {
	.g_volatile_ctrl = gc2145_g_volatile_ctrl,
	.s_ctrl = gc2145_s_ctrl,
};

static const struct v4l2_subdev_core_ops gc2145_core_ops = {
	.s_power = gc2145_s_power,
};

static const struct v4l2_subdev_video_ops gc2145_video_ops = {
	.g_frame_interval	= gc2145_s_g_frame_interval,
	.s_frame_interval	= gc2145_s_g_frame_interval,
	.s_stream		= gc2145_s_stream,
};

static const struct v4l2_subdev_pad_ops gc2145_pad_ops = {
	.init_cfg		= gc2145_init_cfg,
	.enum_mbus_code		= gc2145_enum_mbus_code,
	.get_fmt		= gc2145_get_fmt,
	.set_fmt		= gc2145_set_fmt,
	.enum_frame_size	= gc2145_enum_frame_size,
	.enum_frame_interval	= gc2145_enum_frame_interval,
};

static const struct v4l2_subdev_ops gc2145_subdev_ops = {
	.core	= &gc2145_core_ops,
	.video	= &gc2145_video_ops,
	.pad	= &gc2145_pad_ops,
};

static int gc2145_mode_init(struct gc2145_dev *sensor)
{
	const struct gc2145_mode_info *init_mode;

#if 1
GLS("%s >>> \n",__func__);
	/* set initial mode */
//reg use sensor_fmt_yuv422_yvyu
	sensor->fmt.code = MEDIA_BUS_FMT_UYVY8_2X8;
	sensor->fmt.width = 800;
	sensor->fmt.height = 600;
	sensor->fmt.field = V4L2_FIELD_NONE;
	sensor->fmt.colorspace = V4L2_COLORSPACE_SRGB;

	sensor->frame_interval.denominator = gc2145_FRAME_RATE;
	sensor->frame_interval.numerator = 1;

	init_mode = &gc2145_mode_init_data;

	sensor->current_mode = init_mode;

	sensor->mode_pending_changes = true;

GLS("%s <<< \n",__func__);
#endif
	return 0;
}

static int gc2145_link_setup(struct media_entity *entity,
                           const struct media_pad *local,
                           const struct media_pad *remote, u32 flags)
{
        GLS(" %s \n",__func__);
        return 0;
}

static const struct media_entity_operations gc2145_sd_media_ops = {
        .link_setup = gc2145_link_setup,
};


static int gc2145_v4l2_register(struct gc2145_dev *sensor)
{
	const struct v4l2_ctrl_ops *ops = &gc2145_ctrl_ops;
	struct gc2145_ctrls *ctrls = &sensor->ctrls;
	struct v4l2_ctrl_handler *hdl = &ctrls->handler;
	int ret = 0;

GLS("%s >>> \n",__func__);
	v4l2_i2c_subdev_init(&sensor->sd, sensor->i2c_client,
			     &gc2145_subdev_ops);

//#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
//	sensor->sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
//#endif
	sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_EVENTS;
	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	sensor->sd.entity.ops = &gc2145_sd_media_ops;
	sensor->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	ret = media_entity_pads_init(&sensor->sd.entity, 1, &sensor->pad);
	if (ret < 0)
		return ret;

	v4l2_ctrl_handler_init(hdl, 7);//32); //7

	hdl->lock = &sensor->lock;

	ctrls->vflip = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_VFLIP, 0, 1, 1, 0);
	ctrls->hflip = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_HFLIP, 0, 1, 1, 0);

	ctrls->test_pattern = v4l2_ctrl_new_std_menu_items(hdl,
					&gc2145_ctrl_ops, V4L2_CID_TEST_PATTERN,
					ARRAY_SIZE(test_pattern_menu) - 1,
					0, 0, test_pattern_menu);

	ctrls->auto_exp = v4l2_ctrl_new_std_menu(hdl, ops,
						 V4L2_CID_EXPOSURE_AUTO,
						 V4L2_EXPOSURE_MANUAL, 0,
						 V4L2_EXPOSURE_AUTO);

	ctrls->exposure = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_EXPOSURE,
					    0, 32767, 1, 0);

	ctrls->auto_gain = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_AUTOGAIN,
					     0, 1, 1, 1);
	ctrls->gain = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_GAIN, 0, 2047, 1, 0);

	if (hdl->error) {
		ret = hdl->error;
		goto cleanup_entity;
	}

	ctrls->gain->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrls->exposure->flags |= V4L2_CTRL_FLAG_VOLATILE;

	v4l2_ctrl_auto_cluster(2, &ctrls->auto_gain, 0, true);
	v4l2_ctrl_auto_cluster(2, &ctrls->auto_exp, 1, true);

	sensor->sd.ctrl_handler = hdl;

	//ret = v4l2_async_register_subdev(&sensor->sd);
	ret = v4l2_async_register_subdev_sensor_common(&sensor->sd);
	if (ret < 0)
		goto cleanup_entity;

GLS("%s <<< \n",__func__);
	return 0;

cleanup_entity:
	media_entity_cleanup(&sensor->sd.entity);
	v4l2_ctrl_handler_free(hdl);

	return ret;
}
static int gc2145_check_id(struct gc2145_dev *sensor)
{
	struct device *dev = gc2145_to_dev(sensor);
	u32 chip_id;
	unsigned char val;
	int ret;
GLS("%s >>> \n",__func__);
	gc2145_power_on(sensor);

	chip_id = 0 ;
	val = 0 ;

	ret = gc2145_read_reg(sensor, 0xf0, &val);
	if (ret < 0) {
		dev_err(dev, "failed to read chip id high\n");
		return -ENODEV;
	}
	GLS(" val=0x%x \n", val);
	chip_id |= (val<< 8);
	
	val = 0 ;
	ret = gc2145_read_reg(sensor, 0xf1, &val);
	if (ret < 0) {
		dev_err(dev, "failed to read chip id low\n");
		return -ENODEV;
	}
	GLS(" val=0x%x \n", val);
	chip_id |= (val);
	if (chip_id != gc2145_CHIP_ID) {
		dev_err(dev, "chip id: 0x%04x does not match expected 0x%04x\n",
			chip_id, gc2145_CHIP_ID);
		return -ENODEV;
	}

GLS("%s chipid = %04x <<< \n",__func__, chip_id);
	return 0;
}

static int gc2145_parse_dt(struct gc2145_dev *sensor)
{
	struct device *dev = gc2145_to_dev(sensor);
	int ret;

//	int csi2_noe;
//	int csi2_sel;

GLS("%s >>> \n",__func__);
	/* request power down pin */
    sensor->powdn_gpio = of_get_named_gpio(dev->of_node, "powdn-gpios", 0);
    if (!gpio_is_valid(sensor->powdn_gpio))
    	dev_warn(dev, "No sensor pwdn pin available");
    else {
        ret = devm_gpio_request_one(dev, sensor->powdn_gpio,  GPIOD_OUT_HIGH, "gc2145_mipi_pwdn");
        if (ret < 0) {
        	dev_warn(dev, "Failed to set power pin\n");
        	dev_warn(dev, "retval=%d\n", ret);
        	return ret;
        }
    }

/* get system clock (xclk) */
        sensor->xclk = devm_clk_get(dev, "xclk");
        if (IS_ERR(sensor->xclk)) {
                dev_err(dev, "failed to get xclk\n");
                return PTR_ERR(sensor->xclk);
        }

        sensor->xclk_freq = clk_get_rate(sensor->xclk);
	GLS("%s xclk_freq=%u \n",__func__,sensor->xclk_freq);


    /* request reset pin */
    sensor->reset_gpio = of_get_named_gpio(dev->of_node, "reset-gpios", 0);
    if (!gpio_is_valid(sensor->reset_gpio))
    	dev_warn(dev, "No sensor reset pin available");
    else {
       ret = devm_gpio_request_one(dev, sensor->reset_gpio, GPIOD_OUT_HIGH, "gc2145_mipi_reset");
       if (ret < 0) {
       		dev_warn(dev, "Failed to set reset pin\n");
            return ret;
       }
    }

#if 0
	/* request dovdd pin */
    sensor->dovdd_gpio = of_get_named_gpio(dev->of_node, "dovdd-gpios", 0);
    if (!gpio_is_valid(sensor->dovdd_gpio))
    	dev_warn(dev, "No sensor dovdd pin available");
    else {
       ret = devm_gpio_request_one(dev, sensor->dovdd_gpio, GPIOD_OUT_HIGH, "gc2145_mipi_dovdd");
       if (ret < 0) {
       		dev_warn(dev, "Failed to set dovdd pin\n");
            return ret;
       }
    }

	/* request avdd pin */
    sensor->avdd_gpio = of_get_named_gpio(dev->of_node, "avdd-gpios", 0);
    if (!gpio_is_valid(sensor->avdd_gpio))
    	dev_warn(dev, "No sensor avdd pin available");
    else {
       ret = devm_gpio_request_one(dev, sensor->avdd_gpio, GPIOD_OUT_HIGH, "gc2145_mipi_avdd");
       if (ret < 0) {
       		dev_warn(dev, "Failed to set avdd pin\n");
            return ret;
       }
    }

	csi2_sel = of_get_named_gpio_flags(dev->of_node, "sel-gpios", 0, NULL);
	if (gpio_is_valid(csi2_sel)) {
		ret = devm_gpio_request_one(dev, csi2_sel, GPIOD_OUT_HIGH, NULL);
		if (ret < 0) {
       		dev_warn(dev, "Failed to set csi2_sel pin\n");
            return ret;
       }
		gpio_direction_output(csi2_sel, 1);
	}

	csi2_noe = of_get_named_gpio_flags(dev->of_node, "noe-gpios", 0, NULL);
	if (gpio_is_valid(csi2_noe)) {
		ret = devm_gpio_request_one(dev, csi2_noe, GPIOD_OUT_HIGH, NULL);
		if (ret < 0) {
       		dev_warn(dev, "Failed to set csi2_noe pin\n");
            return ret;
       }
		gpio_direction_output(csi2_noe, 0);
	}
#endif	
GLS("%s <<< \n",__func__);
	return 0;
}

static int gc2145_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct gc2145_dev *sensor;
	int ret;

GLS("%s >>> \n",__func__);
	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;

	sensor->i2c_client = client;

	ret = gc2145_parse_dt(sensor);
	if (ret < 0)
		return -EINVAL;

	ret = gc2145_mode_init(sensor);
	if (ret < 0)
		return ret;

 


	mutex_init(&sensor->lock);

	ret = gc2145_check_id(sensor);
	if (ret < 0)
		goto lock_destroy;

	ret = gc2145_v4l2_register(sensor);
	if (ret < 0)
		goto lock_destroy;

	dev_info(dev, "gc2145 init correctly\n");
GLS("%s <<< okokokok!!!!\n",__func__);

	return 0;

lock_destroy:
	dev_err(dev, "gc2145 init fail: %d\n", ret);
	mutex_destroy(&sensor->lock);

	return ret;
}

static int gc2145_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2145_dev *sensor = to_gc2145_dev(sd);

	v4l2_async_unregister_subdev(&sensor->sd);
	mutex_destroy(&sensor->lock);
	media_entity_cleanup(&sensor->sd.entity);
	v4l2_ctrl_handler_free(&sensor->ctrls.handler);

	return 0;
}

static int __maybe_unused gc2145_suspend(struct device *dev)
{
#if 0
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2145_dev *sensor = to_gc2145_dev(sd);

	if (sensor->is_streaming)
		gc2145_stream_disable(sensor);
#endif
	return 0;
}

static int __maybe_unused gc2145_resume(struct device *dev)
{
#if 0
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2145_dev *sensor = to_gc2145_dev(sd);
	int ret;

GLS("%s >>> \n",__func__);
	if (sensor->is_streaming) {
		ret = gc2145_stream_enable(sensor);
		if (ret < 0)
			goto stream_disable;
	}


GLS("%s <<< \n",__func__);
	return 0;

stream_disable:
//	gc2145_stream_disable(sensor);
	sensor->is_streaming = false;

	return ret;
	#else
	return 0;
	#endif
}

static const struct dev_pm_ops gc2145_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(gc2145_suspend, gc2145_resume)
};

static const struct of_device_id gc2145_dt_ids[] = {
	{ .compatible = "gc2145" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, gc2145_dt_ids);

static struct i2c_driver gc2145_i2c_driver = {
	.driver = {
		.name  = "gc2145",
		.pm = &gc2145_pm_ops,
		.of_match_table	= of_match_ptr(gc2145_dt_ids),
	},
	.probe_new	= gc2145_probe,
	.remove		= gc2145_remove,
};
module_i2c_driver(gc2145_i2c_driver);

MODULE_AUTHOR("gaoliang");
MODULE_DESCRIPTION("A low-level driver for GalaxyCore gc2145 sensors");
MODULE_LICENSE("GPL");

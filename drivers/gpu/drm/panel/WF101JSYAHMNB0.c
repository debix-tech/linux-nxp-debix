// SPDX-License-Identifier: GPL-2.0
/*
 * HUALIN WF101JSYAHMNB0 MIPI-DSI panel driver
 *
 * Copyright 2019 NXP
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/media-bus-format.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

/* Panel specific color-format bits */
#define COL_FMT_16BPP 0x55
#define COL_FMT_18BPP 0x66
#define COL_FMT_24BPP 0x77

/* Write Manufacture Command Set Control */
#define WRMAUCCTR 0xFE

/* Manufacturer Command Set pages (CMD2) */
struct cmd_set_entry {
	u8 len; //param len
	u8 cmd;
	u8 param1;
	u8 param2;
	u8 param3;
};

/*
 * There is no description in the Reference Manual about these commands.
 * We received them from vendor, so just use them as is.
 */
#if 1
static const struct cmd_set_entry init_reg[] = {
{0x3, 0xFF,0x98,0x81,0x03},

//GIP_1

{0x1, 0x01,0x00},
{0x1, 0x02,0x00},
{0x1, 0x03,0x54},        //STVA=STV0
{0x1, 0x04,0xD4},        //STVB=STV1_2
{0x1, 0x05,0x00},        //STVC
{0x1, 0x06,0x11},        //STVA_Rise
{0x1, 0x07,0x09},        //STVB_Rise
{0x1, 0x08,0x00},        //STVC_Rise
{0x1, 0x09,0x00},        //FTI1R(A)
{0x1, 0x0a,0x00},        //FTI2R(B)
{0x1, 0x0b,0x00},        //FTI3R(C)
{0x1, 0x0c,0x00},        //FTI1F(A)
{0x1, 0x0d,0x00},        //FTI2F(B)
{0x1, 0x0e,0x00},        //FTI2F(C)
{0x1, 0x0f,0x26},        //Duty=45%  //CLW1(ALR) 
{0x1, 0x10,0x26},        //Duty=45%  //CLW2(ARR)
{0x1, 0x11,0x00},           
{0x1, 0x12,0x00},        
{0x1, 0x13,0x00},        //CLWX(ATF)
{0x1, 0x14,0x00},
{0x1, 0x15,0x00},        //GPMRi(ALR)
{0x1, 0x16,0x00},        //GPMRii(ARR)
{0x1, 0x17,0x00},        //GPMFi(ALF)
{0x1, 0x18,0x00},        //GPMFii(AFF)
{0x1, 0x19,0x00},
{0x1, 0x1a,0x00},
{0x1, 0x1b,0x00},   
{0x1, 0x1c,0x00},
{0x1, 0x1d,0x00},
{0x1, 0x1e,0x40},        //CLKA 40笆は C0も笆は(X8把σCLKB)
{0x1, 0x1f,0x80},        //C0
{0x1, 0x20,0x06},        //CLKA_Rise
{0x1, 0x21,0x01},        //CLKA_Fall
{0x1, 0x22,0x00},        //CLKB_Rise(keep toggle惠砞CLK A)
{0x1, 0x23,0x00},        //CLKB_Fall
{0x1, 0x24,0x00},        //CLK keep toggle(AL) 8X┕オ
{0x1, 0x25,0x00},        //CLK keep toggle(AR) 8X┕オ
{0x1, 0x26,0x00},
{0x1, 0x27,0x00},
{0x1, 0x28,0x33},       //CLK Phase
{0x1, 0x29,0x33},       //CLK overlap
{0x1, 0x2a,0x00},  
{0x1, 0x2b,0x00},
{0x1, 0x2c,0x00},       //GCH R
{0x1, 0x2d,0x00},       //GCL R 
{0x1, 0x2e,0x00},       //GCH F        
{0x1, 0x2f,0x00},       //GCL F
{0x1, 0x30,0x00},
{0x1, 0x31,0x00},
{0x1, 0x32,0x00},       //GCH/L ext2/1︽  5E 01:31   5E 00:42
{0x1, 0x33,0x00},
{0x1, 0x34,0x03},       //VDD1&2 non-overlap 03:0us  04:2.62us
{0x1, 0x35,0x00},       //GCH/L 跋丁 00:VS玡 01:VS 10:阁VS 11:frameい       
{0x1, 0x36,0x00},
{0x1, 0x37,0x00},       //GCH/L
{0x1, 0x38,0x96},	//VDD1&2 toggle  96:2.5sec   78:2sec
{0x1, 0x39,0x00},
{0x1, 0x3a,0x00}, 
{0x1, 0x3b,0x00},
{0x1, 0x3c,0x00},
{0x1, 0x3d,0x00},
{0x1, 0x3e,0x00},
{0x1, 0x3f,0x00},
{0x1, 0x40,0x00},
{0x1, 0x41,0x00},
{0x1, 0x42,0x00},
{0x1, 0x43,0x00},       //GCH/L
{0x1, 0x44,0x00},


//GIP_2
{0x1, 0x50,0x00},
{0x1, 0x51,0x23},
{0x1, 0x52,0x45},
{0x1, 0x53,0x67},
{0x1, 0x54,0x89},
{0x1, 0x55,0xab},
{0x1, 0x56,0x01},
{0x1, 0x57,0x23},
{0x1, 0x58,0x45},
{0x1, 0x59,0x67},
{0x1, 0x5a,0x89},
{0x1, 0x5b,0xab},
{0x1, 0x5c,0xcd},
{0x1, 0x5d,0xef},

//GIP_3
{0x1, 0x5e, 0x00},
{0x1, 0x5f, 0x0D},     //FW_CGOUT_L[1]    CLK8
{0x1, 0x60, 0x0D},     //FW_CGOUT_L[2]    CLK8
{0x1, 0x61, 0x0C},     //FW_CGOUT_L[3]    CLK6
{0x1, 0x62, 0x0C},     //FW_CGOUT_L[4]    CLK6
{0x1, 0x63, 0x0F},     //FW_CGOUT_L[5]    CLK4
{0x1, 0x64, 0x0F},     //FW_CGOUT_L[6]    CLK4
{0x1, 0x65, 0x0E},     //FW_CGOUT_L[7]    CLK2
{0x1, 0x66, 0x0E},     //FW_CGOUT_L[8]    CLK2
{0x1, 0x67, 0x08},     //FW_CGOUT_L[9]    STV2
{0x1, 0x68, 0x02},     //FW_CGOUT_L[10]   
{0x1, 0x69, 0x02},     //FW_CGOUT_L[11]     
{0x1, 0x6a, 0x02},     //FW_CGOUT_L[12]  
{0x1, 0x6b, 0x02},     //FW_CGOUT_L[13]   
{0x1, 0x6c, 0x02},     //FW_CGOUT_L[14]      
{0x1, 0x6d, 0x02},     //FW_CGOUT_L[15]   
{0x1, 0x6e, 0x02},     //FW_CGOUT_L[16]   VGL    
{0x1, 0x6f, 0x02},     //FW_CGOUT_L[17]   VGL
{0x1, 0x70, 0x14},     //FW_CGOUT_L[18]   VDDO
{0x1, 0x71, 0x15},     //FW_CGOUT_L[19]   VDDE
{0x1, 0x72, 0x06},     //FW_CGOUT_L[20]   STV0
{0x1, 0x73, 0x02},     //FW_CGOUT_L[21]   
{0x1, 0x74, 0x02},     //FW_CGOUT_L[22]   
  
{0x1, 0x75, 0x0D},     //BW_CGOUT_L[1]   
{0x1, 0x76, 0x0D},     //BW_CGOUT_L[2]    
{0x1, 0x77, 0x0C},     //BW_CGOUT_L[3]    
{0x1, 0x78, 0x0C},     //BW_CGOUT_L[4]    
{0x1, 0x79, 0x0F},     //BW_CGOUT_L[5]     
{0x1, 0x7a, 0x0F},     //BW_CGOUT_L[6]     
{0x1, 0x7b, 0x0E},     //BW_CGOUT_L[7]   
{0x1, 0x7c, 0x0E},     //BW_CGOUT_L[8]    
{0x1, 0x7d, 0x08},     //BW_CGOUT_L[9]      
{0x1, 0x7e, 0x02},     //BW_CGOUT_L[10]
{0x1, 0x7f, 0x02},     //BW_CGOUT_L[11]    
{0x1, 0x80, 0x02},     //BW_CGOUT_L[12]   
{0x1, 0x81, 0x02},     //BW_CGOUT_L[13] 
{0x1, 0x82, 0x02},     //BW_CGOUT_L[14]      
{0x1, 0x83, 0x02},     //BW_CGOUT_L[15]   
{0x1, 0x84, 0x02},     //BW_CGOUT_L[16]      
{0x1, 0x85, 0x02},     //BW_CGOUT_L[17]
{0x1, 0x86, 0x14},     //BW_CGOUT_L[18]
{0x1, 0x87, 0x15},     //BW_CGOUT_L[19]
{0x1, 0x88, 0x06},     //BW_CGOUT_L[20]   
{0x1, 0x89, 0x02},     //BW_CGOUT_L[21]   
{0x1, 0x8A, 0x02},     //BW_CGOUT_L[22]   



//CMD_Page 4
{0x3, 0xFF,0x98,0x81,0x04},

{0x1, 0x6E, 0x3B},           //VGH 18V
{0x1, 0x6F, 0x57},           //reg vcl + pumping ratio VGH=4x VGL=-3x
{0x1, 0x3A, 0x24},           //POWER SAVING
{0x1, 0x8D, 0x1F},           //VGL -12V
{0x1, 0x87, 0xBA},           //ESD
{0x1, 0xB2, 0xD1},
{0x1, 0x88, 0x0B},
{0x1, 0x38, 0x01},      
{0x1, 0x39, 0x00},
{0x1, 0xB5, 0x07},           //gamma bias
{0x1, 0x31, 0x75},
{0x1, 0x3B, 0x98},  			
			
//CMD_Page 1
{0x3, 0xFF,0x98,0x81,0x01},
{0x1, 0x22, 0x0A},          //BGR, SS
{0x1, 0x31, 0x09},          ///Column inversion
{0x1, 0x35, 0x07},          //
{0x1, 0x53, 0x7B},          //VCOM1
{0x1, 0x55, 0x40},          //VCOM2 
{0x1, 0x50, 0x86},          // 4.3V  //95-> 4.5V //VREG1OUT
{0x1, 0x51, 0x82},          //-4.3V  //90->-4.5V //VREG2OUT
{0x1, 0x60, 0x27},          //SDT=2.8 
{0x1, 0x62, 0x20},

//============Gamma START=============

{0x1, 0xA0, 0x00},
{0x1, 0xA1, 0x12},
{0x1, 0xA2, 0x20},
{0x1, 0xA3, 0x13},
{0x1, 0xA4, 0x14},
{0x1, 0xA5, 0x27},
{0x1, 0xA6, 0x1D},
{0x1, 0xA7, 0x1F},
{0x1, 0xA8, 0x7C},
{0x1, 0xA9, 0x1D},
{0x1, 0xAA, 0x2A},
{0x1, 0xAB, 0x6B},
{0x1, 0xAC, 0x1A},
{0x1, 0xAD, 0x18},
{0x1, 0xAE, 0x4E},
{0x1, 0xAF, 0x24},
{0x1, 0xB0, 0x2A},
{0x1, 0xB1, 0x4D},
{0x1, 0xB2, 0x5B},
{0x1, 0xB3, 0x23},



//Neg Register
{0x1, 0xC0, 0x00},
{0x1, 0xC1, 0x13},
{0x1, 0xC2, 0x20},
{0x1, 0xC3, 0x12},
{0x1, 0xC4, 0x15},
{0x1, 0xC5, 0x28},
{0x1, 0xC6, 0x1C},
{0x1, 0xC7, 0x1E},
{0x1, 0xC8, 0x7B},
{0x1, 0xC9, 0x1E},
{0x1, 0xCA, 0x29},
{0x1, 0xCB, 0x6C},
{0x1, 0xCC, 0x1A},
{0x1, 0xCD, 0x19},
{0x1, 0xCE, 0x4D},
{0x1, 0xCF, 0x22},
{0x1, 0xD0, 0x2A},
{0x1, 0xD1, 0x4D},
{0x1, 0xD2, 0x5B},
{0x1, 0xD3, 0x23},


//============ Gamma END===========			
//{0x1, FF,03,98,81,04
//{0x1, 2F,01,01    //bist		

//CMD_Page 0			
{0x3, 0xFF, 0x98,0x81,0x00},
 
	
};
#else
static const struct cmd_set_entry init_reg[] = {
 {0x3,0xFF,0x98,0x81,0x03},

//GIP_1
{0x1, 0x01,0x00},           
{0x1, 0x02,0x00},           
{0x1, 0x03,0x55},           
{0x1, 0x04,0x55},           
{0x1, 0x05,0x03},           
{0x1, 0x06,0x06},           
{0x1, 0x07,0x00},           
{0x1, 0x08,0x07},           
{0x1, 0x09,0x00},           
{0x1, 0x0A,0x00},           
{0x1, 0x0B,0x00},           
{0x1, 0x0C,0x00},           
{0x1, 0x0D,0x00},           
{0x1, 0x0E,0x00},           
{0x1, 0x0F,0x00},           
{0x1, 0x10,0x00},           
{0x1, 0x11,0x00},           
{0x1, 0x12,0x00},           
{0x1, 0x13,0x00},           
{0x1, 0x14,0x00}, 
          
{0x1 ,0x15,0x00},           
{0x1 ,0x16,0x00},           
{0x1 ,0x17,0x00},           
{0x1 ,0x18,0x00},           
{0x1 ,0x19,0x00},           
{0x1 ,0x1A,0x00},           
{0x1 ,0x1B,0x00},           
{0x1 ,0x1C,0x00},           
{0x1 ,0x1D,0x00},           
{0x1 ,0x1E,0xC0},           
{0x1 ,0x1F,0x80},           
{0x1 ,0x20,0x04},           
{0x1 ,0x21,0x03},           
{0x1 ,0x22,0x00},           
{0x1 ,0x23,0x00},           
{0x1 ,0x24,0x00},           
{0x1 ,0x25,0x00},           
{0x1 ,0x26,0x00},           
{0x1 ,0x27,0x00},           
{0x1 ,0x28,0x33},           
{0x1 ,0x29,0x33},           
{0x1 ,0x2A,0x00},           
{0x1 ,0x2B,0x00},           
{0x1 ,0x2C,0x00},           
{0x1 ,0x2D,0x00},          
{0x1 ,0x2E,0x00},          
{0x1 ,0x2F,0x00},           
{0x1 ,0x30,0x00},           
{0x1 ,0x31,0x00},           
{0x1 ,0x32,0x00},           
{0x1 ,0x33,0x00},           
{0x1 ,0x34,0x04}, // GPWR1/2 non overlap time 2.62us           
{0x1 ,0x35,0x00},           
{0x1 ,0x36,0x00},           
{0x1 ,0x37,0x00},           
{0x1 ,0x38,0x3C},           
{0x1 ,0x39,0x00},           
{0x1 ,0x3A,0x00},           
{0x1 ,0x3B,0x00},           
{0x1 ,0x3C,0x00},           
{0x1 ,0x3D,0x00},           
{0x1 ,0x3E,0x00},           
{0x1 ,0x3F,0x00},           
{0x1 ,0x40,0x00},           
{0x1 ,0x41,0x00},           
{0x1 ,0x42,0x00},           
{0x1 ,0x43,0x00},           
{0x1 ,0x44,0x00},

//GIP_2           
{0x1, 0x50,0x00},           
{0x1, 0x51,0x11},           
{0x1, 0x52,0x44},           
{0x1, 0x53,0x55},           
{0x1, 0x54,0x88},           
{0x1, 0x55,0xAB},           
{0x1, 0x56,0x00},           
{0x1, 0x57,0x11},           
{0x1, 0x58,0x22},           
{0x1, 0x59,0x33},           
{0x1, 0x5A,0x44},           
{0x1, 0x5B,0x55},           
{0x1, 0x5C,0x66},           
{0x1, 0x5D,0x77},     

//GIP_3  
{0x1, 0x5E,0x00},           
{0x1, 0x5F,0x02},           
{0x1, 0x60,0x02},           
{0x1, 0x61,0x0A},           
{0x1, 0x62,0x09},           
{0x1, 0x63,0x08},           
{0x1, 0x64,0x13},           
{0x1, 0x65,0x12},           
{0x1, 0x66,0x11},           
{0x1, 0x67,0x10},           
{0x1, 0x68,0x0F},           
{0x1, 0x69,0x0E},           
{0x1, 0x6A,0x0D},           
{0x1, 0x6B,0x0C},           
{0x1, 0x6C,0x06},           
{0x1, 0x6D,0x07},           
{0x1, 0x6E,0x02},           
{0x1, 0x6F,0x02},           
{0x1, 0x70,0x02},           
{0x1, 0x71,0x02},           
{0x1, 0x72,0x02},           
{0x1, 0x73,0x02},           
{0x1, 0x74,0x02},           
{0x1, 0x75,0x02},           
{0x1, 0x76,0x02},           
{0x1, 0x77,0x0A},           
{0x1, 0x78,0x06},           
{0x1, 0x79,0x07},          
{0x1, 0x7A,0x10},           
{0x1, 0x7B,0x11},           
{0x1, 0x7C,0x12},           
{0x1, 0x7D,0x13},           
{0x1, 0x7E,0x0C},           
{0x1, 0x7F,0x0D},           
{0x1, 0x80,0x0E},           
{0x1, 0x81,0x0F},           
{0x1, 0x82,0x09},           
{0x1, 0x83,0x08},           
{0x1, 0x84,0x02},           
{0x1, 0x85,0x02},           
{0x1, 0x86,0x02},           
{0x1, 0x87,0x02},           
{0x1, 0x88,0x02},           
{0x1, 0x89,0x02},           
{0x1, 0x8A,0x02},

//Page 4 command,           
{0x3, 0xFF,0x98,0x81,0x04},           
 
//{0x3B,0xC0},     // ILI4003D sel
{0x1, 0x6C,0x15},     //Set VCORE voltage =1.5V           
{0x1, 0x6E,0x2A},    //VGH 15V         
{0x1, 0x6F,0x35},    //reg vcl + pumping ratio VGH=3x VGL=-2.5 & 3x  
{0x1, 0x3A,0x24},    //POWER SAVING
{0x1, 0x8D,0x19},    //VGL -11V
{0x1, 0x87,0xBA},    //ESD
{0x1, 0xB2,0xD1},           
{0x1, 0x88,0x0B},
{0x1, 0x38,0x01},
{0x1, 0x39,0x00},
{0x1, 0xB5,0x02},     //gamma bias
{0x1, 0x31,0x25},     //source bias
{0x1, 0x3B,0x98},


 // Page 1 command             
{0x3, 0xFF,0x98,0x81,0x01},           
{0x1, 0x22,0x0A},        //BGR, SS           
{0x1, 0x31,0x0C},        //Zigzag type3 inversion  Column inversion         
{0x1, 0x53,0x4D},       // ILI4003D sel          
{0x1, 0x55,0x4E},                 
{0x1, 0x50,0xAE},       //VREG1OUT:4.8V             
{0x1, 0x51,0xA9},       //VREG2OUT:-4.8V               
{0x1, 0x60,0x07},       //24=2.5us,28=2.8us,2C/D=3.0us               
       

// Gamma P      
{0x1, 0xA0,0x00},    
{0x1, 0xA1,0x0E},        //VP251         
{0x1, 0xA2,0x1A},        //VP247 
{0x1, 0xA3,0x11},        //VP243         
{0x1, 0xA4,0x14},        //VP239         
{0x1, 0xA5,0x26},        //VP231         
{0x1, 0xA6,0x1B},        //VP219         
{0x1, 0xA7,0x1D},        //VP203         
{0x1, 0xA8,0x68},        //VP175         
{0x1, 0xA9,0x1B},        //VP144         
{0x1, 0xAA,0x27},        //VP111         
{0x1, 0xAB,0x5A},        //VP80          
{0x1, 0xAC,0x1C},        //VP52          
{0x1, 0xAD,0x1A},        //VP36          
{0x1, 0xAE,0x50},        //VP24          
{0x1, 0xAF,0x24},        //VP16          
{0x1, 0xB0,0x29},        //VP12          
{0x1, 0xB1,0x4C},        //VP8           
{0x1, 0xB2,0x5D},        //VP4           
{0x1, 0xB3,0x3F},        //VP0          

// Gamma N      
{0x1, 0xC0,0x00},        //VN255 GAMMA N           
{0x1, 0xC1,0x0E},        //VN251         
{0x1, 0xC2,0x1A},        //VN247         
{0x1, 0xC3,0x11},        //VN243         
{0x1, 0xC4,0x14},        //VN239         
{0x1, 0xC5,0x26},        //VN231         
{0x1, 0xC6,0x1B},        //VN219         
{0x1, 0xC7,0x1D},        //VN203         
{0x1, 0xC8,0x68},        //VN175         
{0x1, 0xC9,0x1B},        //VN144         
{0x1, 0xCA,0x27},        //VN111         
{0x1, 0xCB,0x5A},        //VN80          
{0x1, 0xCC,0x1C},        //VN52          
{0x1, 0xCD,0x1A},        //VN36          
{0x1, 0xCE,0x50},        //VN24          
{0x1, 0xCF,0x24},        //VN16          
{0x1, 0xD0,0x29},        //VN12          
{0x1, 0xD1,0x4C},        //VN8           
{0x1, 0xD2,0x5D},        //VN4           
{0x1, 0xD3,0x3F},        //VN0
{0x3, 0xFF,0x98,0x81,0x00},
{0x1, 0x35,0x00},  // TE On
  
};
#endif

static const u32 rad_bus_formats[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_RGB666_1X18,
	MEDIA_BUS_FMT_RGB565_1X16,
};

static const u32 rad_bus_flags = DRM_BUS_FLAG_DE_LOW |
				 DRM_BUS_FLAG_PIXDATA_SAMPLE_POSEDGE;

struct rad_panel {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;

	struct gpio_desc *reset;
	struct backlight_device *backlight;
	struct backlight_device *backlight2;

	struct regulator_bulk_data *supplies;
	unsigned int num_supplies;

	bool prepared;
	bool enabled;

	const struct rad_platform_data *pdata;
};

struct rad_platform_data {
	int (*enable)(struct rad_panel *panel);
};

static const struct drm_display_mode default_mode = {
#if 0
	.clock = 51200,
        .hdisplay = 1024,
        .hsync_start = 1024 + 160,
        .hsync_end = 1024 + 160 + 4,
        .htotal = 1024 + 160 + 4 + 156,
        .vdisplay = 600,
        .vsync_start = 600 + 17,
        .vsync_end = 600 + 17 + 1,
        .vtotal = 600 + 17 + 1 + 17,

#endif
#if 1
	//.clock = 42105,
	.clock = 43500,
        .hdisplay = 1024,
        .hsync_start = 1024 + 30,
        .hsync_end = 1024 + 30 + 30,
        .htotal = 1024 + 30 + 30 + 30,
        .vdisplay = 600,
        .vsync_start = 600 + 10,
        .vsync_end = 600 + 10 + 10,
        .vtotal = 600 + 10 + 10 + 10,

#endif
#if 0
	.clock = 51450,
	.hdisplay = 1024,
	.hsync_start = 1024 + 156,
	.hsync_end = 1024 + 156 + 8,
	.htotal = 1024 + 156 + 8 + 156,
	.vdisplay = 600,
	.vsync_start = 600 + 16,
	.vsync_end = 600 + 16 + 6,
	.vtotal = 600 + 16 + 6 + 16,
#endif
	.width_mm = 107,
	.height_mm = 172,

	.flags = DRM_MODE_FLAG_NHSYNC |
		 DRM_MODE_FLAG_NVSYNC,
};

static inline struct rad_panel *to_rad_panel(struct drm_panel *panel)
{
	return container_of(panel, struct rad_panel, panel);
}

static int rad_panel_push_cmd_list(struct mipi_dsi_device *dsi,
				   struct cmd_set_entry const *cmd_set,
				   size_t count)
{
	size_t i;
	int ret = 0;

	for (i = 0; i < count; i++) {
		const struct cmd_set_entry *entry = cmd_set++;
		if(entry->len==1){
			u8 buffer2[2] = { entry->cmd, entry->param1 };

			ret = mipi_dsi_generic_write(dsi, &buffer2, sizeof(buffer2));
		}else if(entry->len==3){
			u8 buffer4[4] = { entry->cmd, entry->param1, entry->param2, entry->param3};
			//printk("GLS_MIPI set %02x %02x %02x %02x \n", entry->cmd, entry->param1, entry->param2, entry->param3);
			//printk("GLS_MIPI set %02x %02x %02x %02x \n", buffer4[0], buffer4[1], buffer4[2], buffer4[3]);

			ret = mipi_dsi_generic_write(dsi, &buffer4, sizeof(buffer4));
		
		}
		if (ret < 0)
			return ret;
	}

	return ret;
};

static int color_format_from_dsi_format(enum mipi_dsi_pixel_format format)
{
	switch (format) {
	case MIPI_DSI_FMT_RGB565:
		return COL_FMT_16BPP;
	case MIPI_DSI_FMT_RGB666:
	case MIPI_DSI_FMT_RGB666_PACKED:
		return COL_FMT_18BPP;
	case MIPI_DSI_FMT_RGB888:
		return COL_FMT_24BPP;
	default:
		return COL_FMT_24BPP; /* for backward compatibility */
	}
};

static int rad_panel_prepare(struct drm_panel *panel)
{
	struct rad_panel *rad = to_rad_panel(panel);
	int ret;

	if (rad->prepared)
		return 0;

	ret = regulator_bulk_enable(rad->num_supplies, rad->supplies);
	if (ret)
		return ret;

	/* At lest 10ms needed between power-on and reset-out as RM specifies */
	usleep_range(10000, 12000);

	rad->prepared = true;

	return 0;
}

static int rad_panel_unprepare(struct drm_panel *panel)
{
	struct rad_panel *rad = to_rad_panel(panel);
	int ret;

	if (!rad->prepared)
		return 0;

	ret = regulator_bulk_disable(rad->num_supplies, rad->supplies);
	if (ret)
		return ret;

	rad->prepared = false;

	return 0;
}

static int mipi_enable(struct rad_panel *panel)
{
	struct mipi_dsi_device *dsi = panel->dsi;
	struct device *dev = &dsi->dev;
	int color_format = color_format_from_dsi_format(dsi->format);
	int ret;

	if (panel->enabled)
		return 0;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	gpiod_set_value_cansleep(panel->reset, 1);
	msleep(5);
	gpiod_set_value_cansleep(panel->reset, 0);
	msleep(10);
	gpiod_set_value_cansleep(panel->reset, 1);
	msleep(120);
/*
	ret = rad_panel_push_cmd_list(dsi,
				      &init_reg[0],
				      ARRAY_SIZE(init_reg));
	if (ret < 0) {
		dev_err(dev, "Failed to send MCS (%d)\n", ret);
		goto fail;
	} */

	/* Set pixel format */
	ret = mipi_dsi_dcs_set_pixel_format(dsi, color_format);
	if (ret < 0) {
		dev_err(dev, "Failed to set pixel format (%d)\n", ret);
		goto fail;
	}
	/* Exit sleep mode */
	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode (%d)\n", ret);
		goto fail;
	}

	msleep(120);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display ON (%d)\n", ret);
		goto fail;
	}
	msleep(100);
	ret = mipi_dsi_dcs_set_tear_on(dsi, 1);
	if (ret < 0) {
		dev_err(dev, "GLS_MIPI Failed to set display ON (%d)\n", ret);
	}
	

	backlight_enable(panel->backlight);
	backlight_enable(panel->backlight2);

	panel->enabled = true;

	return 0;

fail:

	return ret;
}

static int rad_panel_enable(struct drm_panel *panel)
{
	struct rad_panel *rad = to_rad_panel(panel);

	return rad->pdata->enable(rad);
}

static int rad_panel_disable(struct drm_panel *panel)
{
	struct rad_panel *rad = to_rad_panel(panel);
	struct mipi_dsi_device *dsi = rad->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	if (!rad->enabled)
		return 0;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	backlight_disable(rad->backlight);
	backlight_disable(rad->backlight2);

	usleep_range(10000, 12000);

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display OFF (%d)\n", ret);
		return ret;
	}

	usleep_range(5000, 10000);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode (%d)\n", ret);
		return ret;
	}

	rad->enabled = false;

	return 0;
}

static int rad_panel_get_modes(struct drm_panel *panel,
			       struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_err(panel->dev, "failed to add mode %ux%u@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	connector->display_info.bus_flags = rad_bus_flags;

	drm_display_info_set_bus_formats(&connector->display_info,
					 rad_bus_formats,
					 ARRAY_SIZE(rad_bus_formats));
	return 1;
}

static int rad_bl_get_brightness(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	struct rad_panel *rad = mipi_dsi_get_drvdata(dsi);
	u16 brightness;
	int ret;

	if (!rad->prepared)
		return 0;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_get_display_brightness(dsi, &brightness);
	if (ret < 0)
		return ret;

	bl->props.brightness = brightness;

	return brightness & 0xff;
}
static int delayBright = 1;
static int rad_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	struct rad_panel *rad = mipi_dsi_get_drvdata(dsi);
	int ret = 0;


	if (!rad->prepared)
		return 0;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_brightness(dsi, bl->props.brightness);
	if (ret < 0)
		return ret;

	if (rad->backlight2) {
		if(delayBright==1){
			delayBright = 0;
			msleep(100);
		}
                rad->backlight2->props.power = FB_BLANK_UNBLANK;
		
		rad->backlight2->props.brightness = bl->props.brightness;
                backlight_update_status(rad->backlight2);
        }


	return 0;
}

static const struct backlight_ops rad_bl_ops = {
	.update_status = rad_bl_update_status,
	.get_brightness = rad_bl_get_brightness,
};

static const struct drm_panel_funcs rad_panel_funcs = {
	.prepare = rad_panel_prepare,
	.unprepare = rad_panel_unprepare,
	.enable = rad_panel_enable,
	.disable = rad_panel_disable,
	.get_modes = rad_panel_get_modes,
};

static const char * const rad_supply_names[] = {
	"v3p3",
	"v1p8",
};

static int rad_init_regulators(struct rad_panel *rad)
{
	struct device *dev = &rad->dsi->dev;
	int i;

	rad->num_supplies = ARRAY_SIZE(rad_supply_names);
	rad->supplies = devm_kcalloc(dev, rad->num_supplies,
				     sizeof(*rad->supplies), GFP_KERNEL);
	if (!rad->supplies)
		return -ENOMEM;

	for (i = 0; i < rad->num_supplies; i++)
		rad->supplies[i].supply = rad_supply_names[i];

	return devm_regulator_bulk_get(dev, rad->num_supplies, rad->supplies);
};

static const struct rad_platform_data rad_mipi = {
	.enable = &mipi_enable,
};

static const struct of_device_id rad_of_match[] = {
	{ .compatible = "debix,WF101JSYAHMNB0", .data = &rad_mipi },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rad_of_match);

static int rad_panel_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct of_device_id *of_id = of_match_device(rad_of_match, dev);
	struct device_node *np = dev->of_node;
	struct rad_panel *panel;
	struct backlight_properties bl_props;
	int ret;
	struct device_node *backlight2;
	u32 video_mode;

	if (!of_id || !of_id->data)
		return -ENODEV;

	panel = devm_kzalloc(&dsi->dev, sizeof(*panel), GFP_KERNEL);
	if (!panel)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, panel);

	panel->dsi = dsi;
	panel->pdata = of_id->data;

	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_NO_EOT_PACKET;
	backlight2 = of_parse_phandle(dev->of_node, "backlight", 0);
        if (backlight2) {
                panel->backlight2 = of_find_backlight_by_node(backlight2);
                of_node_put(backlight2);
        }

	ret = of_property_read_u32(np, "video-mode", &video_mode);
	if (!ret) {
		switch (video_mode) {
		case 0:
			/* burst mode */
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_BURST |
					   MIPI_DSI_MODE_VIDEO;
			break;
		case 1:
			/* non-burst mode with sync event */
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO;
			break;
		case 2:
			/* non-burst mode with sync pulse */
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
					   MIPI_DSI_MODE_VIDEO;
			break;
		case 3:
			/* command mode */
			dsi->mode_flags |= MIPI_DSI_CLOCK_NON_CONTINUOUS |
					   MIPI_DSI_MODE_VSYNC_FLUSH;
			break;
		default:
			dev_warn(dev, "invalid video mode %d\n", video_mode);
			break;
		}
	}

	ret = of_property_read_u32(np, "dsi-lanes", &dsi->lanes);
	if (ret) {
		dev_err(dev, "Failed to get dsi-lanes property (%d)\n", ret);
		return ret;
	}

	panel->reset = devm_gpiod_get_optional(dev, "reset",
					       GPIOD_OUT_LOW |
					       GPIOD_FLAGS_BIT_NONEXCLUSIVE);
	if (IS_ERR(panel->reset)) {
		ret = PTR_ERR(panel->reset);
		dev_err(dev, "Failed to get reset gpio (%d)\n", ret);
		return ret;
	}
	gpiod_set_value_cansleep(panel->reset, 1);

	memset(&bl_props, 0, sizeof(bl_props));
	bl_props.type = BACKLIGHT_RAW;
	bl_props.brightness = 255;
	bl_props.max_brightness = 255;

	panel->backlight = devm_backlight_device_register(dev, dev_name(dev),
							  dev, dsi, &rad_bl_ops,
							  &bl_props);
	if (IS_ERR(panel->backlight)) {
		ret = PTR_ERR(panel->backlight);
		dev_err(dev, "Failed to register backlight (%d)\n", ret);
		return ret;
	}

	ret = rad_init_regulators(panel);
	if (ret)
		return ret;

	drm_panel_init(&panel->panel, dev, &rad_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	dev_set_drvdata(dev, panel);

	drm_panel_add(&panel->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret)
		drm_panel_remove(&panel->panel);

	return ret;
}

static void rad_panel_remove(struct mipi_dsi_device *dsi)
{
	struct rad_panel *rad = mipi_dsi_get_drvdata(dsi);
	struct device *dev = &dsi->dev;
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret)
		dev_err(dev, "Failed to detach from host (%d)\n", ret);

	drm_panel_remove(&rad->panel);
}

static void rad_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct rad_panel *rad = mipi_dsi_get_drvdata(dsi);

	rad_panel_disable(&rad->panel);
	rad_panel_unprepare(&rad->panel);
}

static struct mipi_dsi_driver rad_panel_driver = {
	.driver = {
		.name = "WF101JSYAHMNB0",
		.of_match_table = rad_of_match,
	},
	.probe = rad_panel_probe,
	.remove = rad_panel_remove,
	.shutdown = rad_panel_shutdown,
};
module_mipi_dsi_driver(rad_panel_driver);

MODULE_AUTHOR("John gao  <john@polyhex.com>");
MODULE_DESCRIPTION("DRM Driver for HUALIN WF101JSYAHMNB0  MIPI DSI panel");
MODULE_LICENSE("GPL v2");

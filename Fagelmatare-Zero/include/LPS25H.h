/*
This file is part of experix, an experiment and process control interface.
Copyright (C) 2004-2016 William Bayard McConnaughey

experix is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the Licence, or
(at your option) any later version.

experix is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public Licence for more details.

You should have received a copy of the GNU General Public Licence
along with experix.  If not, see <http://www.gnu.org/licenses/>.

Programming definitions for the LPS25H MEMS 260-1260 hPa pressure sensor
(STMicroelectronics NV)
Reference: datasheet DM00066332.pdf (January 2014), www.st.com
Very terse comments are provided in this file. See explanations in the
datasheet. This header is specialized for Raspberry Pi sense-hat usage.
Names used are mostly composed by prepending "LPS25H_" on the register and
bitfield names used in the datasheet. Terms in ALL CAPS are the register
designations; those with lower-case letters are data values or bitfield
macros.
Registers marked with the comment "//2" are register pairs, where the
designation applies to the least significant byte and the high byte is
addressed by adding "+1". Likewise, "//3" signifies a register triple. (The
data sheet uses two or three definitions with "_XL", "_L" and "_H" on the end).
For bitfields we are using function-style definitions. The ones ending in
"_ef(v)" are for extracting field values from a register value, and the "_if(f)"
ones are for inserting field values into a register value. Example: to make
the LPS25H_AV_CONF setting for AVGP=3 and AVGT=2, use this expression:
LPS25H_AV_CONF_AVGT_if(2)|LPS25H_AV_CONF_AVGP_if(3)
Field length in bits is shown by comments such as  //2b
Note that these macros do not check for inappropriate submitted values.  
Note well, regarding register addresses not mentioned herein: from the
datasheet, "Registers marked as Reserved must not be changed. The Writing to
those registers may cause permanent damages to the device. The content of the
registers that are loaded at boot should not be changed. They contain the
factory calibration values. Their content is automatically restored when the
device is powered-up."
*/
/* Notes on Raspberry Pi sense-hat use
CS (pin 6) is high: I2C interface is used, with GPIO3 to SCL/SPC (pin 2) and
GPIO2 to SDA/SDI/SDO (pin 4). INT1 (pin 7) goes only to a test-point. SDO/SA0
(pin 5) is grounded, so LSB of device address is 0.
*/
#define LPS25H_SAD          0x5c // slave address for device on sense-hat
#define LPS25H_WHO_AM_I     0x0f  // read this to get--
#define LPS25H_who_am_i     0xbd  //   this device identifier

#define LPS25H_reg_auto     0x80  // OR with register addr. to auto-advance

#define LPS25H_REF_P        0x08  //3  Reference pressure

#define LPS25H_RES_CONF     0x10  // Pressure and temperature resolution mode
#define LPS25H_AV_CONF_AVGT_if(f) ((f)<<2)   //2b temperature. averaging mode
#define LPS25H_AV_CONF_AVGP_if(f) (f)        //2b pressure averaging mode

#define LPS25H_CTRL_REG1    0x20
#define LPS25H_CTRL_REG1_PD_if(f)       ((f)<<7) //1b powerdown:  1= device active
#define LPS25H_CTRL_REG1_ODR_if(f)      ((f)<<4) //3b output data rate (table 18)
#define LPS25H_CTRL_REG1_DIFF_EN_if(f)  ((f)<<3) //1b enable diff. pressure
#define LPS25H_CTRL_REG1_BDU_if(f)      ((f)<<2) //1b block data update
#define LPS25H_CTRL_REG1_RESET_AZ_if(f) ((f)<<1) //1b pressure reset autozero
#define LPS25H_CTRL_REG1_SIM_if(f)      (f)      //1b SPI serial interface mode

#define LPS25H_CTRL_REG2    0x21
#define LPS25H_CTRL_REG2_BOOT_if(f)     ((f)<<7)  //1b refresh registers from flash
#define LPS25H_CTRL_REG2_FIFO_EN_if(f)  ((f)<<6)  //1b enable FIFO
#define LPS25H_CTRL_REG2_WTM_EN_if(f)   ((f)<<5)  //1b enable FIFO watermark
#define LPS25H_CTRL_REG2_FIFO_MEAN_DEC_if(f) ((f)<<4) //1b enable 1 Hz ODR decim.
#define LPS25H_CTRL_REG2_SWRESET_if(f)  ((f)<<2)  //1b software reset (with BOOT=1)
#define LPS25H_CTRL_REG2_AUTO_ZERO_if(f) ((f)<<1) //1b copy PRESS_OUT to REF_P
#define LPS25H_CTRL_REG2_ONE_SHOT_if(f) (f)     //1b with ODR=0, start new conv.

#define LPS25H_CTRL_REG3    0x22     // (sense-hat: INT1 not used)
#define LPS25H_CTRL_REG3_INT_H_L_if(f)  ((f)<<7)  //1b interrupt active hi/lo
#define LPS25H_CTRL_REG3_PP_OD_if(f)    ((f)<<6)  //1b push-pull/open-drain select
#define LPS25H_CTRL_REG3_INT1_S_if(f)   (f)     //2b INT pad signal (table 19)

#define LPS25H_CTRL_REG4    0x23     // (sense-hat: INT1 not used)
#define LPS25H_CTRL_REG4_P1_EMPTY_if(f) ((f)<<3)  //1b Empty signal on INT1
#define LPS25H_CTRL_REG4_P1_WTM_if(f)   ((f)<<2)  //1b Watermark signal on INT1
#define LPS25H_CTRL_REG4_P1_Overun_if(f) ((f)<<1) //1b Overrun signal on INT1
#define LPS25H_CTRL_REG4_P1_DRDY_if(f)  (f)       //1b Data ready signal on INT1

#define LPS25H_INT_CFG      0x24
#define LPS25H_INT_CFG_LIR_if(f)   ((f)<<2) //1b latch int. source into INT_SOURCE
#define LPS25H_INT_CFG_PL_E_if(f)  ((f)<<1) //1b enable int. on diff. press. low
#define LPS25H_INT_CFG_PH_E_if(f)  (f)      //1b enable int. on diff. press. high

#define LPS25H_INT_SOURCE   0x25
#define LPS25H_INT_SOURCE_IA_ef(v)   (((v)>>2)&1) //1b interrupt active
#define LPS25H_INT_SOURCE_PL_ef(v)   (((v)>>1)&1) //1b differential pressure low
#define LPS25H_INT_SOURCE_PH_ef(v)   ((v)&1)      //1b differential pressure high

#define LPS25H_STATUS_REG   0x27
#define LPS25H_STATUS_REG_P_OR_ef(v) (((v)>>5)&1) //1b pressure data overrun
#define LPS25H_STATUS_REG_T_OR_ef(v) (((v)>>4)&1) //1b temperature data overrun
#define LPS25H_STATUS_REG_P_DA_ef(v) (((v)>>1)&1) //1b pressure data available
#define LPS25H_STATUS_REG_T_DA_ef(v) ((v)&1)      //1b temperature data available

#define LPS25H_PRESS_POUT   0x28  //3 pressure data, 3 bytes at 0x28,0x29,0x2a
#define LPS25H_TEMP_OUT     0x2b  //2 temperature data, 2 bytes at 0x2b,0x2c

#define LPS25H_FIFO_CTRL    0x2e  // datasheet sec 8: FIFO operating details
#define LPS25H_FIFO_CTRL_F_MODE_if(f)    ((f)<<5) //3b FIFO mode, tables 20,22,23
#define LPS25H_FIFO_CTRL_WTM_POINT_if(f) (f)    //5b watermark select, table 21

#define LPS25H_FIFO_STATUS  0x2f
#define LPS25H_FIFO_STATUS_WTM_FIFO_ef(v)   ((v)>>7)     //1b watermark status
#define LPS25H_FIFO_STATUS_FULL_FIFO_ef(v)  (((v)>>6)&1) //1b overrun bit status
#define LPS25H_FIFO_STATUS_EMPTY_FIFO_ef(v) (((v)>>5)&1) //1b empty FIFO
#define LPS25H_FIFO_STATUS_DIFF_POINT_ef(v) ((v)&0x1f)   //5b FIFO data level

#define LPS25H_THS_P        0x30  //2  threshold pressure for interrupt
#define LPS25H_RPDS         0x39  //2  pressure offset after soldering


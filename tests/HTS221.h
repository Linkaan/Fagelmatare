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

Programming definitions for the HTS221 capacitive digital sensor for
relative humidity and temperature (STMicroelectronics NV)
Reference: datasheet DM00116291.pdf (October 2015), www.st.com
Very terse comments are provided in this file. See explanations in the
datasheet. This header is specialized for Raspberry Pi sense-hat usage.
Names used are mostly composed by prepending "HTS221_" on the register and
bitfield names used in the datasheet. Terms in ALL CAPS are the register
designations; those with lower-case letters are data values or bitfield
macros.
Registers marked with the comment "//2" are register pairs, where the
designation applies to the least significant byte and the high byte is
addressed by adding "+1". (The data sheet uses two definitions with "_L"
and "_H" on the end).
For bitfields we are using function-style definitions. The ones ending in
"_ef(v)" are for extracting field values from a register value, and the
"_if(f)" ones are for inserting fields into a register value. Example: to make
the HTS221_AV_CONF setting for AVGH=6 and AVGT=2, use this expression:
HTS221_AV_CONF_AVGT_if(2)|HTS221_AV_CONF_AVGH_if(6)
Field length in bits is shown by comments such as  //2b
Note that these macros do not check for inappropriate submitted values.  
Note well, regarding register addresses not mentioned herein: from the
datasheet, "Registers marked as Reserved must not be changed. Writing to
those registers may cause permanent damage to the device."
*/
/* Notes on Raspberry Pi sense-hat use
CS (pin 6) is high: I2C interface is used, with GPIO3 to SCL/SPC (pin 2) and
GPIO2 to SDA/SDI/SDO (pin 4). DRDY (pin 3) goes only to a test-point.
*/
#define HTS221_SAD          0x5f  // slave address
#define HTS221_WHO_AM_I     0x0f  // read this to get--
#define HTS221_who_am_i     0xbc  //   this device identifier

#define HTS221_reg_auto     0x80  // OR with register addr. to auto-advance

// humidity and temerature resolution mode:  see datasheet table 16
#define HTS221_AV_CONF      0x10
#define HTS221_AV_CONF_AVGT_if(f) ((f)<<3)      //3b temperature avg. mode
#define HTS221_AV_CONF_AVGH_if(f) (f)           //3b humidity avg. mode

#define HTS221_CTRL_REG1    0x20
#define HTS221_CTRL_REG1_PD_if(f) ((f)<<7)      //1b 0= powerdown, 1= active
#define HTS221_CTRL_REG1_BDU_if(f) ((f)<<2)     //1b 0= continuous update
#define HTS221_CTRL_REG1_ODR_if(f) (f)          //2b output data rate

#define HTS221_CTRL_REG2    0x21
#define HTS221_CTRL_REG2_BOOT_if(f) ((f)<<7)    //1b 1= reboot memory content
#define HTS221_CTRL_REG2_HEATER_if(f) ((f)<<1)  //1b 1= enable heater
#define HTS221_CTRL_REG2_ONE_SHOT_if(f) (f)     //1b 1= start for a new dataset

// note: DRDY pin is not being used in the sense-hat
#define HTS221_CTRL_REG3    0x22
#define HTS221_CTRL_REG3_DRDY_H_L_if(f) ((f)<<7) //1b DRDY active hi/lo
#define HTS221_CTRL_REG3_PP_OD_if(f) ((f)<<6)    //1b push-pull / open-drain
#define HTS221_CTRL_REG3_DRDY_EN_if(f) ((f)<<2)  //1b enable DRDY

#define HTS221_STATUS_REG   0x27
#define HTS221_STATUS_REG_H_DA_ef(v) (((v)>>1)&1) //1b new humidity avail.
#define HTS221_STATUS_REG_T_DA_ef(v) ((v)&1)      //1b new temperature avail.

#define HTS221_HUMIDITY_OUT 0x28  //2  humidity data out
#define HTS221_TEMP_OUT     0x2a  //2  temperature data out

/* See datasheet discussion on use of calibration registers and values.
DO NOT MODIFY these registers. Names are made by prepending "HTS221_CAL_"
onto the register names in table 19, except where that would not be usable. */
#define HTS221_CAL_H0_rH_x2   0x30
#define HTS221_CAL_H1_rH_x2   0x31
#define HTS221_CAL_T0_degC_x8 0x32
#define HTS221_CAL_T1_degC_x8 0x33
#define HTS221_CAL_T1_T0_msb  0x35
#define HTS221_CAL_H0_T0_OUT  0x36  //2
#define HTS221_CAL_H1_T0_OUT  0x3a  //2
#define HTS221_CAL_T0_OUT     0x3c  //2
#define HTS221_CAL_T1_OUT     0x3e  //2


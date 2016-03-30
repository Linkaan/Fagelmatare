/*
gcc -g -O -Wall -o temperature temperature.c

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

Synopsis:   sudo ./temperature a1 a2 a3 a4 a5 a6 a7 a8 a9 a10 a11 a12
See the header files LPS25H.h, HTS221.h, LSM9DS1.h and the device
datasheets to understand the meaning of these arguments.
arguments in this order:                       (variable name)
a1 integer 1..1000  number of measurements (samplecount)
a2 integer 1..100000000  microseconds between measurements (sample_usec)
a3 integer 0..3     LPS25H temperature averaging mode (LPS25HifAVGT)
a4 integer 0..3     LPS25H pressure averaging mode (LPS25HifAVGP)
a5 integer 0..4     LPS25H output data rate (LPS25HifODR)
a6 integer 0..7     HTS221 temperature averaging mode (HTS221ifAVGT)
a7 integer 0..7     HTS221 humidity averaging mode (HTS221ifAVGH)
a8 integer 0..3     HTS221 output data rate (HTS221ifODR)
a9 integer 0 or 1   put LSM9DS1 temperature data in FIFO (LSM_Tfifo)
a10 integer 0 or 1  imitate RTIMULibDrive11 reading HTS221 calibration
                 (HTS221_CAL_persnickitty)
a11 integer 0 or 1  imitate RTIMULibDrive11 operating LSM9DS1
                 (LSM9DS1_persnickitty)
a12 integer 0 or 1  see code marked with "?p?"  (general_persnickitty)
   Procedures tend to follow RTIMULibDrive11, except that improvements
   are being tried in these places.

The program prints a result table with this header:
  sample   LPS25H    deg C     HTS221    deg C     LSM9DS1   deg C     CPU        deg C    P  PF H  A1 A2
to present this information:
sample number
LPS25H temperature result, raw hexadecimal and converted to centigrade
HTS221 temperature result, raw hexadecimal and converted to centigrade
LSM9DS1 temperature result, raw hexadecimal and converted tocentigrade
CPU temperature from /sys/devices/virtual/thermal/thermal_zone0/temp,
  raw hexadecimal and converted by dividing by 1000
status registers: P=LPS25H_STATUS_REG; PF=LPS25H_FIFO_STATUS;
H=HTS221_STATUS_REG; A1=LSM9DS1_AG_STATUS_17; A2=LSM9DS1_AG_STATUS_27
*/
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <asm/types.h>
#include "sense-hat.h"

int samplecount;
int sample_usec; // limit 100000000 (100 sec)
int LPS25HifAVGT, LPS25HifAVGP, LPS25HifODR;
int HTS221_CAL_persnickitty, LSM9DS1_persnickitty, general_persnickitty;
__s16 *HTS221d16, *LSM9DS1d16;
__s32 *CPUd32, *LPS25Hd16;
unsigned char HTS221cal[16];
int HTS221ifAVGT, HTS221ifAVGH, HTS221ifODR, LSM_Tfifo;
double T0_degC, T1_degC, H0_T0_OUT, H1_T0_OUT, T0_OUT, T1_OUT, H0, H1;
double TdegD, ToutD, Hm, Hc;
__u8 LPS25H_status, LPS25H_fifo_status, HTS221_status;
__u8 LSM9DS1_AG_status17, LSM9DS1_AG_status27;
char cpuTp[] = "/sys/devices/virtual/thermal/thermal_zone0/temp";
char i2cDp[] = "/dev/i2c-1";

double T_LPS25H, P_LPS25H, T_HTS221, H_HTS221, T_LSM9DS1, T_CPU;
void show_temperature( int ii )
{
  //T_LPS25H = 42.5 + ((double)(LPS25Hd16[ii]))/480.0;
  P_LPS25H = ((float)(LPS25Hd16[ii]))/(float)4096;
  //T_HTS221 = T0_degC + (((double)(HTS221d16[ii])-T0_OUT)/ToutD)*TdegD;//T0_degC + (((double)(HTS221d16[ii])-T0_OUT)/ToutD)*TdegD;
  H_HTS221 = (double)(HTS221d16[ii]) * Hm + Hc;
  /* LSM9DS1 datasheet Table 5: typical sensitivity = 16 lsb per deg C; footnote
  says output is 0 (typ) at 25 C. So use T_LSM9DS1 = 25+<LSM9DS1d16>/16 */
  T_LSM9DS1 = 25.0 + ((double)(LSM9DS1d16[ii]))/16.0;
  /*??? I found something on the forum that seems to say CPU temperature is the
  number from the /sys file divided by 1000 */
  T_CPU = ((double)(CPUd32[ii])) / 1000.0;
  printf( "%8d   0x%04x %+9.5g    0x%04x %+9.5g    0x%04x %+9.5g    0x%08x%+9.5g   %02x %02x %02x %02x %02x\n",
    ii, (__u16)(LPS25Hd16[ii]), P_LPS25H, (__u16)(HTS221d16[ii]), H_HTS221,
        (__u16)( LSM9DS1d16[ii]), T_LSM9DS1, CPUd32[ii], T_CPU,
        LPS25H_status, LPS25H_fifo_status, HTS221_status,
        LSM9DS1_AG_status17, LSM9DS1_AG_status27 );
}

int main( int argc, char **argv )
{
  int cpuT, i2c, res, ii;
  struct timespec ts;
  unsigned char buf[16];

  if ( argc != 13 ) {
    printf( "incorrect number of arguments\n" );
    return 1;
  }
  samplecount = strtol( argv[1], NULL, 0 );
  sample_usec = strtol( argv[2], NULL, 0 );
  LPS25HifAVGT = strtol( argv[3], NULL, 0 );
  LPS25HifAVGT = strtol( argv[4], NULL, 0 );
  LPS25HifODR = strtol( argv[5], NULL, 0 );
  HTS221ifAVGT = strtol( argv[6], NULL, 0 );
  HTS221ifAVGH = strtol( argv[7], NULL, 0 );
  HTS221ifODR = strtol( argv[8], NULL, 0 );
  LSM_Tfifo = strtol( argv[9], NULL, 0 );
  HTS221_CAL_persnickitty = strtol( argv[10], NULL, 0 );
  LSM9DS1_persnickitty = strtol( argv[11], NULL, 0 );
  general_persnickitty = strtol( argv[12], NULL, 0 );
  if ( (samplecount < 1) || (samplecount > 1000) ||
       (sample_usec < 1) || (sample_usec > 100000000) ||
       (LPS25HifAVGT < 0) || (LPS25HifAVGT > 3) ||
       (LPS25HifAVGP < 0) || (LPS25HifAVGP > 3) ||
       (LPS25HifODR < 0) || (LPS25HifODR > 4)   ||
       (HTS221ifAVGT < 0) || (HTS221ifAVGT > 7) ||
       (HTS221ifAVGH < 0) || (HTS221ifAVGH > 7) ||
       (HTS221ifODR < 0) || (HTS221ifODR > 3) ||
       ((LSM_Tfifo != 0) && (LSM_Tfifo != 1)) ||
       ((HTS221_CAL_persnickitty != 0 ) && (HTS221_CAL_persnickitty != 1)) ||
       ((LSM9DS1_persnickitty != 0) && (LSM9DS1_persnickitty != 1)) ||
       ((general_persnickitty != 0) && (general_persnickitty != 1)) ) {
    printf( "bad parameter values\n" );
    return 1;
  }
  // open CPU temperature sys file
  cpuT = open( cpuTp, O_RDONLY );
  if ( cpuT == -1 ) {
    perror( "open cpuTp" );
    return 1;
  }
  // open i2c dev file
  i2c = open( i2cDp, O_RDWR );
  if ( i2c == -1 ) {
    perror( "open i2c" );
    return 1;
  }
  // discover LPS25H
  res = ioctl( i2c, I2C_SLAVE, LPS25H_SAD );
  buf[0] = LPS25H_WHO_AM_I;
  res = write( i2c, buf, 1 );
  if ( res == -1 ) {
    perror( "write i2c" );
    return 1;
  }
  res = read( i2c, buf, 1 );
  if ( res != 1 ) {
    if ( res == -1 ) perror( "read i2c" );
    else printf( "read i2c returns %d\n", res );
    return 1;
  }
  if ( buf[0] != LPS25H_who_am_i ) {
    printf( "expect LPS25H id = 0x%x, get 0x%x\n", LPS25H_who_am_i, buf[0] );
    return 1;
  }
  // Set up for temperature measurements using the LPS25H
  // Set LPS25H_CTRL_REG1 following usage in RTIMULibDrive11
  buf[0] = LPS25H_CTRL_REG1;
  buf[1] = LPS25H_CTRL_REG1_PD_if(1) |      // power up
         LPS25H_CTRL_REG1_ODR_if(LPS25HifODR) | // output data rate
         LPS25H_CTRL_REG1_DIFF_EN_if(0) |   // disable differential pressure
         LPS25H_CTRL_REG1_BDU_if(1) |       // enable block update
         LPS25H_CTRL_REG1_RESET_AZ_if(0) |  // do not auto-zero
         LPS25H_CTRL_REG1_SIM_if(0);        // SPI mode (irrelevant for i2c)
  res = write( i2c, buf, 2 );
  if ( res != 2 ) {
    perror( "i2c write LPS25H" );
    return 1;
  }
  /* Set temperature and pressure averaging modes: internal averaging numbers
                                       for mode = 0,  1,   2,   3
  LPS25HifAVGP      pressure averaging number     8, 32, 128, 512
  LPS25HifAVGT      temperature averaging number  8, 16,  32,  64  */
  buf[0] = LPS25H_RES_CONF;
  buf[1] = LPS25H_AV_CONF_AVGT_if(LPS25HifAVGT) |
           LPS25H_AV_CONF_AVGP_if(LPS25HifAVGP);
  res = write( i2c, buf, 2 );
  /* Set FIFO mode. (The FIFO holds pressure data so this should not make a
  difference for temperature.) */
  buf[0] = LPS25H_FIFO_CTRL;
  buf[1] = LPS25H_FIFO_CTRL_F_MODE_if(6) |    // running average
           LPS25H_FIFO_CTRL_WTM_POINT_if(1);  // average 2 samples
  res = write( i2c, buf, 2 );
  // Set LPS25H_CTRL_REG2 following usage in RTIMULibDrive11
  buf[0] = LPS25H_CTRL_REG2;
  buf[1] = LPS25H_CTRL_REG2_BOOT_if(0) |      // no refresh registers from flash
           LPS25H_CTRL_REG2_FIFO_EN_if(1) |   // enable FIFO
           LPS25H_CTRL_REG2_WTM_EN_if(0) |    // no enable FIFO watermark
           LPS25H_CTRL_REG2_FIFO_MEAN_DEC_if(0) | // no enable 1 Hz ODR decim.
           LPS25H_CTRL_REG2_SWRESET_if(0) |   // no software reset (with BOOT=1)
           LPS25H_CTRL_REG2_AUTO_ZERO_if(0) | // no copy PRESS_OUT to REF_P
           LPS25H_CTRL_REG2_ONE_SHOT_if(0);   // no do one-shot here
  /* LPS25H_CTRL_REG3, LPS25H_CTRL_REG4, LPS25H_INT_CFG are irrelevant since
     interrupts are not being used. */
  // discover HTS221
  res = ioctl( i2c, I2C_SLAVE, HTS221_SAD );
  buf[0] = HTS221_WHO_AM_I;
  res = write( i2c, buf, 1 );
  if ( res != 1 ) {
    perror( "i2c write HTS221" );
    return 1;
  }
  res = read( i2c, buf, 1 );
  if ( res != 1 ) {
    if ( res == -1 ) perror( "read i2c" );
    else printf( "read i2c returns %d\n", res );
    return 1;
  }
  if ( buf[0] != HTS221_who_am_i ) {
    printf( "expect HTS221 id = 0x%x, get 0x%x\n", HTS221_who_am_i, buf[0] );
    return 1;
  }
  // Set up for temperature measurements using the HTS221
  // Set HTS221_CTRL_REG1 following usage in RTIMULibDrive11
  buf[0] = HTS221_CTRL_REG1;                                         //Drive11
  buf[1] = HTS221_CTRL_REG1_PD_if(1) |            // power up             1
           HTS221_CTRL_REG1_BDU_if(1) |           // enable block update  1
           HTS221_CTRL_REG1_ODR_if(HTS221ifODR);  // output data rate     3
  res = write( i2c, buf, 2 );
  /* Set temperature and humidity averaging modes: internal averaging numbers
                                   for mode = 0, 1,  2,  3,  4,   5,   6,   7
  HTS221ifAVGH  humidity averaging number     4, 8, 16, 32, 64, 128, 256, 512
  HTS221ifAVGT  temperature averaging number  2, 4,  8, 16, 32,  64, 128, 256 */
  buf[0] = HTS221_AV_CONF;                                           //Drive11
  buf[1] = HTS221_AV_CONF_AVGT_if(HTS221ifAVGT) |                    //   3
           HTS221_AV_CONF_AVGH_if(HTS221ifAVGH);                     //   3
  res = write( i2c, buf, 2 );
  /* Read the calibration registers and calculate conversion coefficients.
  See datasheet tables 19 and 20. */
  if ( HTS221_CAL_persnickitty != 0 ) {
    // do it exactly the way we saw in the strace output
    buf[0] = HTS221_CAL_T1_T0_msb | HTS221_reg_auto;
    res = write( i2c, buf, 1 );
    res = read( i2c, buf, 1 );  HTS221cal[5] = buf[0];
    buf[0] = HTS221_CAL_T0_degC_x8 | HTS221_reg_auto;
    res = write( i2c, buf, 1 );
    res = read( i2c, buf, 1 );  HTS221cal[2] = buf[0];
    buf[0] = HTS221_CAL_T1_degC_x8 | HTS221_reg_auto;
    res = write( i2c, buf, 1 );
    res = read( i2c, buf, 1 );  HTS221cal[3] = buf[0];
    buf[0] = HTS221_CAL_T0_OUT | HTS221_reg_auto;
    res = write( i2c, buf, 1 );
    res = read( i2c, buf, 2 );
    ((__s16 *)HTS221cal)[6] = ((__s16 *)buf)[0];
    buf[0] = HTS221_CAL_T1_OUT | HTS221_reg_auto;
    res = write( i2c, buf, 1 );
    res = read( i2c, buf, 2 );
    ((__s16 *)HTS221cal)[7] = ((__s16 *)buf)[0];
    buf[0] = HTS221_CAL_H0_rH_x2 | HTS221_reg_auto;
    res = write( i2c, buf, 1 );
    res = read( i2c, buf, 1 );  HTS221cal[0] = buf[0];
    buf[0] = HTS221_CAL_H1_rH_x2 | HTS221_reg_auto;
    res = write( i2c, buf, 1 );
    res = read( i2c, buf, 1 );  HTS221cal[1] = buf[0];
    buf[0] = HTS221_CAL_H0_T0_OUT | HTS221_reg_auto;
    res = write( i2c, buf, 1 );
    res = read( i2c, buf, 2 );
    ((__s16 *)HTS221cal)[3] = ((__s16 *)buf)[0];
    buf[0] = HTS221_CAL_H1_T0_OUT | HTS221_reg_auto;
    res = write( i2c, buf, 1 );
    res = read( i2c, buf, 2 );
    ((__s16 *)HTS221cal)[5] = ((__s16 *)buf)[0];
  } else {
    // do it what looks like a more efficient way
    buf[0] = HTS221_CAL_H0_rH_x2 | HTS221_reg_auto;
    res = write( i2c, buf, 1 );
    res  = read( i2c, HTS221cal, 16 );
    if ( res != 16 ) {
      if ( res == -1 ) perror( "HTS221_CAL_H0_rH_x2" );
      else printf( "read 16 at HTS221_CAL_H0_rH_x2 returns %d\n", res );
    }
  }
  /* The datasheet has a muddled and confusing discussion of temperature
  conversion. I hope this is correct. */
  T0_degC = ((double)((int)(HTS221cal[2]) | (((int)(HTS221cal[5]&3))<<8)))/8.0;
  T1_degC = ((double)((int)(HTS221cal[3]) | (((int)(HTS221cal[5]&12))<<6)))/8.0;
  H0_T0_OUT = (double)(((__s16 *)HTS221cal)[3]);
  H1_T0_OUT = (double)(((__s16 *)HTS221cal)[5]);
  H0 = ((double)(((__u8 *)HTS221cal)[0]))/2.0;
  H1 = ((double)(((__u8 *)HTS221cal)[1]))/2.0;
  T0_OUT = (double)(((__s16 *)HTS221cal)[6]);
  T1_OUT = (double)(((__s16 *)HTS221cal)[7]);
  Hm = (H1-H0)/(H1_T0_OUT-H0_T0_OUT);
  Hc = (H0)-(Hm*H0_T0_OUT);
  ToutD = T1_OUT - T0_OUT;
  TdegD = T1_degC - T0_degC;
  printf( "HTS221 cal = %02x %02x %02x %02x %02x %02x %02x %02x "
                       "%02x %02x %02x %02x %02x %02x %02x %02x\n",
    HTS221cal[0], HTS221cal[1], HTS221cal[2], HTS221cal[3],
    HTS221cal[4], HTS221cal[5], HTS221cal[6], HTS221cal[7],
    HTS221cal[8], HTS221cal[9], HTS221cal[10], HTS221cal[11],
    HTS221cal[12], HTS221cal[13], HTS221cal[14], HTS221cal[15] );
  printf( "T0_degC=%g T1_degC=%g H0_T0_OUT =%g H1_T0_OUT=%g\n",
           T0_degC,   T1_degC,   H0_T0_OUT,    H1_T0_OUT );
  printf( "T0_OUT=%g  T1_OUT=%g  ToutD=%g      TdegD=%g\n",
           T0_OUT,    T1_OUT,    ToutD,        TdegD );
  printf( "H0=%g      H1=%g      Hm=%g         Hc=%g\n",
           H0,        H1,        Hm,           Hc );
  // Set up for temperature measurements using the LSM9DS1
  if ( LSM9DS1_persnickitty != 0 ) {
    // Do everything with this unit that RTIMULibDrive11 did
    res = ioctl( i2c, I2C_SLAVE, LSM9DS1_M_SAD );
    buf[0] = LSM9DS1_M_WHO_AM_I;
    res = write( i2c, buf, 1 );
    if ( res != 1 ) {
      perror( "i2c write LSM9D1_M" );
      return 1;
    }
    res = read( i2c, buf, 1 );
    if ( buf[0] != LSM9DS1_M_who_am_i ) {
      printf( "expect LSM9DS1_M id = 0x%x, get 0x%x\n",
              LSM9DS1_M_who_am_i, buf[0] );
      return 1;
    }
    res = ioctl( i2c, I2C_SLAVE, LSM9DS1_AG_SAD );
    // ?p? do we have to boot it before we can discover it?
    if ( general_persnickitty == 1 ) {
      buf[0] = LSM9DS1_AG_CTRL_REG8;
      buf[1] = LSM9DS1_AG_CTRL_REG8_BOOT_if(1);
      res = write( i2c, buf, 2 );
      if ( res != 2 ) {
        perror( "i2c write LSM9DS1_AG (boot)" );
        return 1;
      }
      ts.tv_sec = 0;
      ts.tv_nsec = 100000000;
      nanosleep( &ts, NULL );
      buf[0] = LSM9DS1_AG_WHO_AM_I;
      res = write( i2c, buf, 1 );
      if ( res != 1 ) {
        perror( "i2c write LSM9D1_AG" );
        return 1;
      }
      res = read( i2c, buf, 1 );
      if ( buf[0] != LSM9DS1_AG_who_am_i ) {
        printf( "expect LSM9DS1_AG id = 0x%x, get 0x%x\n",
                LSM9DS1_AG_who_am_i, buf[0] );
        return 1;
      }
    } else {
      buf[0] = LSM9DS1_AG_WHO_AM_I;
      res = write( i2c, buf, 1 );
      if ( res != 1 ) {
        perror( "i2c write LSM9D1_AG" );
        return 1;
      }
      res = read( i2c, buf, 1 );
      if ( buf[0] != LSM9DS1_AG_who_am_i ) {
        printf( "expect LSM9DS1_AG id = 0x%x, get 0x%x\n",
                LSM9DS1_AG_who_am_i, buf[0] );
        return 1;
      }
      buf[0] = LSM9DS1_AG_CTRL_REG8;
      buf[1] = LSM9DS1_AG_CTRL_REG8_BOOT_if(1);
      res = write( i2c, buf, 2 );
      if ( res != 2 ) {
        perror( "i2c write LSM9DS1_AG (boot)" );
        return 1;
      }
      ts.tv_sec = 0;
      ts.tv_nsec = 100000000;
      nanosleep( &ts, NULL );
    }
    buf[0] = LSM9DS1_AG_CTRL_REG1;
    buf[1] = LSM9DS1_AG_CTRL_REG1_ODR_G_if(3) | // gyro output data rate
             LSM9DS1_AG_CTRL_REG1_FS_G_if(1)  | // gyro full-scale select
             LSM9DS1_AG_CTRL_REG1_BW_G_if(1);    // gyro bandwidth select
    res = write( i2c, buf, 2 );
    buf[0] = LSM9DS1_AG_CTRL_REG3;
    buf[1] = LSM9DS1_AG_CTRL_REG3_LP_mode_if(0) | // low power mode enable
             LSM9DS1_AG_CTRL_REG3_HP_EN_if(1)   | // high-pass filter enable
             LSM9DS1_AG_CTRL_REG3_HPCF_G_if(4);    // gyro high cutoff freq sel
    res = write( i2c, buf, 2 );
    res = ioctl( i2c, I2C_SLAVE, LSM9DS1_M_SAD );
    buf[0] = LSM9DS1_M_WHO_AM_I;
    res = write( i2c, buf, 1 );
    if ( res != 1 ) {
      perror( "i2c write LSM9D1_M" );
      return 1;
    }
    res = read( i2c, buf, 1 );
    if ( buf[0] != LSM9DS1_M_who_am_i ) {
      printf( "expect LSM9DS1_M id = 0x%x, get 0x%x\n",
              LSM9DS1_M_who_am_i, buf[0] );
      return 1;
    }
    res = ioctl( i2c, I2C_SLAVE, LSM9DS1_AG_SAD );
    buf[0] = LSM9DS1_AG_CTRL_REG6_XL;
    buf[1] = LSM9DS1_AG_CTRL_REG6_XL_ODR_XL_if(3) | // out data rate, power
             LSM9DS1_AG_CTRL_REG6_XL_FS_XL_if(3)  | // accel full-scale sel
             LSM9DS1_AG_CTRL_REG6_XL_BW_SCAL_ODR_if(0) | // bandwidth sel
             LSM9DS1_AG_CTRL_REG6_XL_BW_XL_if(3);   // anti-alias filter bw sel
    res = write( i2c, buf, 2 );
    buf[0] = LSM9DS1_AG_CTRL_REG7_XL;
    buf[1] = LSM9DS1_AG_CTRL_REG7_XL_HR_pf(0)   | // hi res mode enable
             LSM9DS1_AG_CTRL_REG7_XL_DCF_if(0)  | // filter cutoff freq sel
             LSM9DS1_AG_CTRL_REG7_XL_FDS_if(0)  | // filtered data sel
             LSM9DS1_AG_CTRL_REG7_XL_HPIS1_if(0);  // hi-pass filter int.en.
    res = write( i2c, buf, 2 );
    res = ioctl( i2c, I2C_SLAVE, LSM9DS1_M_SAD );
    buf[0] = LSM9DS1_M_CTRL_REG1;
    buf[1]= LSM9DS1_M_CTRL_REG1_TEMP_COMP_if(0) | // temperature comp. en.
            LSM9DS1_M_CTRL_REG1_OM_if(0)        | // X,Y operative mode sel
            LSM9DS1_M_CTRL_REG1_DO_if(5)        | // output data rate sel
            LSM9DS1_M_CTRL_REG1_FAST_ODR_if(0)  | // data rates > 80 Hz
            LSM9DS1_M_CTRL_REG1_ST_if(0);          // self-test enable
    res = write( i2c, buf, 2 );
    buf[0] = LSM9DS1_M_CTRL_REG2;
    buf[1] = LSM9DS1_M_CTRL_REG2_FS_if(0)       | // full-scale config
             LSM9DS1_M_CTRL_REG2_REBOOT_if(0)   | // reboot memory content
             LSM9DS1_M_CTRL_REG2_SOFT_RST_if(0); // reset user,config regs
    res = write( i2c, buf, 2 );
    buf[0] = LSM9DS1_M_CTRL_REG3;
    buf[1] = LSM9DS1_M_CTRL_REG3_I2C_DISABLE_if(0) | // disable I2C
             LSM9DS1_M_CTRL_REG3_LP_if(0)       | // low-power mode config
             LSM9DS1_M_CTRL_REG3_SIM_if(0)      | // SPI mode selection
             LSM9DS1_M_CTRL_REG3_MD_if(0);        // operating mode sel
    res = write( i2c, buf, 2 );
    // now do the setup actions we think we need for temperature monitor
    res = ioctl( i2c, I2C_SLAVE, LSM9DS1_AG_SAD );
    buf[0] = LSM9DS1_AG_CTRL_REG8;
    buf[1] = LSM9DS1_AG_CTRL_REG8_BDU_if(1) |   // enable block update
             LSM9DS1_AG_CTRL_REG8_IF_ADD_INC_if(1) |  // register auto advance
             LSM9DS1_AG_CTRL_REG8_BLE_if(0);    // little-endian data
    res = write( i2c, buf, 2 );
    buf[0] = LSM9DS1_AG_CTRL_REG9;
    buf[1] = LSM9DS1_AG_CTRL_REG9_FIFO_TEMP_EN_if(LSM_Tfifo) |
             LSM9DS1_AG_CTRL_REG9_FIFO_EN_if(1);      // enable FIFO
    res = write( i2c, buf, 2 );
    buf[0] = LSM9DS1_AG_FIFO_CTRL;
    buf[1] = LSM9DS1_AG_FIFO_CTRL_FMODE_if(6); // continuous mode
    res = write( i2c, buf, 2 );
  } else {
    // Only deal with the accel/gyro part
    // boot and discover LSM9DS1 accelerometer and gyroscope device
    // ?p? do we have to boot it before we can discover it?
    res = ioctl( i2c, I2C_SLAVE, LSM9DS1_AG_SAD );
    if ( general_persnickitty == 1 ) {
      buf[0] = LSM9DS1_AG_CTRL_REG8;
      buf[1] = LSM9DS1_AG_CTRL_REG8_BOOT_if(1);
      res = write( i2c, buf, 2 );
      if ( res != 2 ) {
        perror( "i2c write LSM9DS1_AG (boot)" );
        return 1;
      }
      ts.tv_sec = 0;
      ts.tv_nsec = 100000000;
      nanosleep( &ts, NULL );
      buf[0] = LSM9DS1_AG_WHO_AM_I;
      res = write( i2c, buf, 1 );
      if ( res != 1 ) {
        perror( "i2c write LSM9D1_AG" );
        return 1;
      }
      res = read( i2c, buf, 1 );
      if ( buf[0] != LSM9DS1_AG_who_am_i ) {
        printf( "expect LSM9DS1_AG id = 0x%x, get 0x%x\n",
                LSM9DS1_AG_who_am_i, buf[0] );
        return 1;
      }
    } else {
      buf[0] = LSM9DS1_AG_WHO_AM_I;
      res = write( i2c, buf, 1 );
      if ( res != 1 ) {
        perror( "i2c write LSM9D1_AG" );
        return 1;
      }
      res = read( i2c, buf, 1 );
      if ( buf[0] != LSM9DS1_AG_who_am_i ) {
        printf( "expect LSM9DS1_AG id = 0x%x, get 0x%x\n",
                LSM9DS1_AG_who_am_i, buf[0] );
        return 1;
      }
      buf[0] = LSM9DS1_AG_CTRL_REG8;
      buf[1] = LSM9DS1_AG_CTRL_REG8_BOOT_if(1);
      res = write( i2c, buf, 2 );
      if ( res != 2 ) {
        perror( "i2c write LSM9DS1_AG (boot)" );
        return 1;
      }
      ts.tv_sec = 0;
      ts.tv_nsec = 100000000;
      nanosleep( &ts, NULL );
    }
    buf[0] = LSM9DS1_AG_CTRL_REG8;
    buf[1] = LSM9DS1_AG_CTRL_REG8_BDU_if(1) |   // enable block update
             LSM9DS1_AG_CTRL_REG8_IF_ADD_INC_if(1) |  // register auto advance
             LSM9DS1_AG_CTRL_REG8_BLE_if(0);    // little-endian data
    res = write( i2c, buf, 2 );
    buf[0] = LSM9DS1_AG_CTRL_REG9;
    buf[1] = LSM9DS1_AG_CTRL_REG9_FIFO_TEMP_EN_if(LSM_Tfifo) |
             LSM9DS1_AG_CTRL_REG9_FIFO_EN_if(1);      // enable FIFO
    res = write( i2c, buf, 2 );
    buf[0] = LSM9DS1_AG_FIFO_CTRL;
    buf[1] = LSM9DS1_AG_FIFO_CTRL_FMODE_if(6); // continuous mode
    res = write( i2c, buf, 2 );
  }
  // Monitor the temperatures
  LPS25Hd16 = malloc( samplecount*sizeof(__s16) );
  HTS221d16 = malloc( samplecount*sizeof(__s16) );
  LSM9DS1d16 = malloc( samplecount*sizeof(__s16) );
  CPUd32 = malloc( samplecount*sizeof(__s32) );
  printf( "Taking %d temperature measurements: wait %g seconds\n",
    samplecount, ((double)samplecount)*((double)sample_usec) / 1000000 );
  printf( "  sample   LPS25H    deg C     HTS221    deg C     LSM9DS1   deg C     CPU          deg C    P  PF H  A1 A2\n" );
  for ( ii = 0;  ii < samplecount;  ii++ ) {
    if ( LPS25HifODR == 0 ) { // issue a one-shot
      res = ioctl( i2c, I2C_SLAVE, LPS25H_SAD );
      buf[0] = LPS25H_CTRL_REG2;
      buf[1] = LPS25H_CTRL_REG2_BOOT_if(0) |      // no refresh registers
               LPS25H_CTRL_REG2_FIFO_EN_if(1) |   // enable FIFO
               LPS25H_CTRL_REG2_WTM_EN_if(0) |    // no enable FIFO watermark
               LPS25H_CTRL_REG2_FIFO_MEAN_DEC_if(0) | // no 1 Hz ODR decim.
               LPS25H_CTRL_REG2_SWRESET_if(0) |   // no software reset
               LPS25H_CTRL_REG2_AUTO_ZERO_if(0) | // no PRESS_OUT --> REF_P
               LPS25H_CTRL_REG2_ONE_SHOT_if(1);   // do one-shot here
      res = write( i2c, buf, 2 );
    }
    if ( HTS221ifODR == 0 ) { // issue a one-shot
      res = ioctl( i2c, I2C_SLAVE, HTS221_SAD );
      buf[0] = HTS221_CTRL_REG2;
      buf[1] = HTS221_CTRL_REG2_BOOT_if(0) |
               HTS221_CTRL_REG2_HEATER_if(0) |
               HTS221_CTRL_REG2_ONE_SHOT_if(1);
      res = write( i2c, buf, 2 );
    }
    // wait out the sample interval
    ts.tv_sec = sample_usec/1000000;
    ts.tv_nsec = (sample_usec%1000000)*1000;
    nanosleep( &ts, NULL );
    // get a LPS25H pressure sample
    res = ioctl( i2c, I2C_SLAVE, LPS25H_SAD );
    buf[0] = LPS25H_STATUS_REG;
    res = write( i2c, buf, 1 );
    res = read( i2c, buf, 2 );
    LPS25H_status = buf[0];
    buf[0] = LPS25H_FIFO_STATUS;
    res = write( i2c, buf, 1 );
    res = read( i2c, buf, 2 );
    LPS25H_fifo_status = buf[0];
    buf[0] = LPS25H_PRESS_POUT|LPS25H_reg_auto;
    //buf[0] = 0x28 + 0x80;
    res = write( i2c, buf, 1 );
    res = read( i2c, buf, 3 );
    LPS25Hd16[ii] = (((__s32)(buf[2])) << 16) | (((__s32)(buf[1])) << 8) | (((__s32)(buf[0])));
    // get a HTS221 humidity sample
    res = ioctl( i2c, I2C_SLAVE, HTS221_SAD );
    buf[0] = HTS221_STATUS_REG;
    res = write( i2c, buf, 1 );
    res = read( i2c, buf, 2 );
    HTS221_status = buf[0];
    buf[0] = HTS221_HUMIDITY_OUT | HTS221_reg_auto;
    res = write( i2c, buf, 1 );
    res = read( i2c, buf, 2 );
    HTS221d16[ii] = (((__s16)buf[1]) << 8) | (__s16)buf[0]);

    // get a LSM9DS1 temperature sample
    res = ioctl( i2c, I2C_SLAVE, LSM9DS1_AG_SAD );
    //todo here: check LSM9DS1_AG_STATUS_REG, LSM9DS1_AG_FIFO_SRC
    buf[0] = LSM9DS1_AG_STATUS_17;
    res = write( i2c, buf, 1 );
    res = read( i2c, buf, 2 );
    LSM9DS1_AG_status17 = buf[0];
    buf[0] = LSM9DS1_AG_STATUS_27;
    res = write( i2c, buf, 1 );
    res = read( i2c, buf, 2 );
    //?p? can we do it with a 2-byte read on ..._L
    if ( general_persnickitty == 1 ) {
      LSM9DS1_AG_status27 = buf[0];
      buf[2] = LSM9DS1_AG_OUT_TEMP_L;
      res = write( i2c, buf+2, 1 );
      res = read( i2c, buf, 1 );
      buf[2] = LSM9DS1_AG_OUT_TEMP_H;
      res = write( i2c, buf+2, 1 );
      res = read( i2c, buf+1, 1 );
    } else {
      LSM9DS1_AG_status27 = buf[0];
      buf[2] = LSM9DS1_AG_OUT_TEMP_L;
      res = write( i2c, buf+2, 1 );
      res = read( i2c, buf, 2 );
    }
    LSM9DS1d16[ii] = ((__s16 *)buf)[0];
    // get a CPU temperature sample
    res = lseek( cpuT, 0, SEEK_SET );
    res = read( cpuT, buf, 16 );
    CPUd32[ii] = strtol( buf, NULL, 0 );
    // make a reasonable compromise between no output for a long time vs.
    // not delaying this loop with the display task
    if ( sample_usec >= 100000 ) show_temperature( ii );
  }
  if ( sample_usec < 100000 )
    for ( ii = 0;  ii < samplecount;  ii++ ) show_temperature(ii);
  return 0;
}

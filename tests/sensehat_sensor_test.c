/*
gcc -g -O -Wall -o basicsensor basicsensor.c

This program is based on experix, an experiment and process control interface.
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
#include "HTS221.h"   // HTS221 relative humidity and temperature sensor
#include "LPS25H.h"   // LPS25H MEMS 260-1260 hPa pressure sensor

// averaging mode and output rate definitions for HTS221 and LPS25H
#define LPS25HifAVGP 3
#define LPS25HifODR 3
#define HTS221ifODR 3
#define HTS221ifAVGT 3
#define HTS221ifAVGH 3

// definitions for i2c-dev
#define DEVPATH_I2C     "/dev/i2c-1"  // the device file
#define I2C_SLAVE       0x0703        // ioctl:  Use this slave address

__s16 HTS221_T_OUT, HTS221_H_OUT;
__s32 LPS25H_P_OUT;
__s16 H0_T0_OUT = 0;
__s16 H1_T0_OUT = 0;
__s16 T0_OUT = 0;
__s16 T1_OUT = 0;
float H0_rH, H1_rH;
float T0_degC, T1_degC;
__u8 LPS25H_status, LPS25H_fifo_status, HTS221_status;
char i2cDp[] = DEVPATH_I2C;

float P_LPS25H, T_HTS221, H_HTS221;
void show_readings() {
  P_LPS25H = (float)(LPS25H_P_OUT) / 4096.0f;
  T_HTS221 = T0_degC + (((float)(HTS221_T_OUT)-T0_OUT)/(T1_OUT-T0_OUT))*(T1_degC-T0_degC);
  H_HTS221 = H0_rH + (((float)(HTS221_H_OUT)-H0_T0_OUT)/(H1_T0_OUT-H0_T0_OUT))*(H1_rH-H0_rH);

  printf( "P_LPS25H  mbar      T_HTS221    deg C     H_HTS221    rH\n" );
  printf( "0x%-8.04x%+-10.5g0x%-10.04x%+-10.5g0x%-10.04x%+-10.5g\n",
         (__u32)(LPS25H_P_OUT), P_LPS25H, (__u16)(HTS221_T_OUT), T_HTS221,
         (__u16)(HTS221_H_OUT), H_HTS221 );
}

int main() {
  int i2c, res;
  unsigned char buf[16];
  unsigned char HTS221cal[16];
  __u16 T0_degC_x8;
  __u16 T1_degC_x8;
  __u8 H0_rH_x2;
  __u8 H1_rH_x2;

  // open the i2c device on raspberry pi
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
  /* Set pressure averaging modes: internal averaging numbers
                                       for mode = 0,  1,   2,   3
  LPS25HifAVGP      pressure averaging number     8, 32, 128, 512  */
  buf[0] = LPS25H_RES_CONF;
  buf[1] = LPS25H_AV_CONF_AVGP_if(LPS25HifAVGP);
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
   buf[0] = HTS221_CAL_H0_rH_x2 | HTS221_reg_auto;
   res = write( i2c, buf, 1 );
   res  = read( i2c, HTS221cal, 16 );
   if ( res != 16 ) {
     if ( res == -1 ) perror( "HTS221_CAL_H0_rH_x2" );
     else printf( "read 16 at HTS221_CAL_H0_rH_x2 returns %d\n", res );
   }

   T0_degC_x8 = (((__u16)HTS221cal[5] & 0x3 ) << 8) | (__u16)HTS221cal[2];
   T1_degC_x8 = (((__u16)HTS221cal[5] & 0xc ) << 6) | (__u16)HTS221cal[3];
   T0_OUT = ((__s16*)(HTS221cal))[6];
   T1_OUT = ((__s16*)(HTS221cal))[7];
   H0_T0_OUT = ((__s16*)(HTS221cal))[3];
   H1_T0_OUT = ((__s16*)(HTS221cal))[5];
   H0_rH_x2 = ((__u8*)(HTS221cal))[0];
   H1_rH_x2 = ((__u8*)(HTS221cal))[1];
   T0_degC = (float)(T0_degC_x8) / 8.0f;
   T1_degC = (float)(T1_degC_x8) / 8.0f;
   H0_rH = (float)(H0_rH_x2) / 2.0f;
   H1_rH = (float)(H1_rH_x2) / 2.0f;

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
   res = write( i2c, buf, 1 );
   res = read( i2c, buf, 3 ); // read registers at 0x28, 0x29, 0x2a
   LPS25H_P_OUT = (((__s32)(buf[2])) << 16) | (((__s32)(buf[1])) << 8) | (((__s32)(buf[0])));
   // get a HTS221 humidity sample
   res = ioctl( i2c, I2C_SLAVE, HTS221_SAD );
   buf[0] = HTS221_STATUS_REG;
   res = write( i2c, buf, 1 );
   res = read( i2c, buf, 2 );
   HTS221_status = buf[0];
   buf[0] = HTS221_HUMIDITY_OUT | HTS221_reg_auto;
   res = write( i2c, buf, 1 );
   res = read( i2c, buf, 2 );
   HTS221_H_OUT = (((__s16)buf[1]) << 8) | (__s16)buf[0];
   buf[0] = HTS221_TEMP_OUT | HTS221_reg_auto;
   res = write( i2c, buf, 1 );
   res = read( i2c, buf, 2 );
   HTS221_T_OUT = (((__s16)buf[1]) << 8) | (__s16)buf[0];

   show_readings();

   close(i2c);
   return 0;
}

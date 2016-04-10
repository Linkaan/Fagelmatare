#ifndef SENSORS_H
#define SENSORS_H

// averaging mode and output rate definitions for HTS221 and LPS25H
#define LPS25HifAVGP 3
#define LPS25HifODR 3
#define HTS221ifODR 3
#define HTS221ifAVGT 3
#define HTS221ifAVGH 3

// definitions for i2c-dev
#define DEVPATH_I2C     "/dev/i2c-1"  // the device file
#define I2C_SLAVE       0x0703        // ioctl:  Use this slave address

// struct to store sensor readings for HTS221, LPS25H
struct IMUData {
  float pressure;
  float temperature;
  float humidity;
};

/*
 * Initialize i2c device and discover LPS25H and HTS221
 * Obtain calculated constants from sensors used in formulas
 */
int sensors_init(void);

/*
 * Grab sensor readings and populate a IMUData struct with
 * median value calculated from LPS25H and HTS221 sensors
 */
int sensors_grab(struct IMUData *, int, int);

#endif

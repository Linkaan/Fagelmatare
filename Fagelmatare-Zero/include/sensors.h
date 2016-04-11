/*
 *  sensors.h
 *    This is a small library that communicates with RPi SenseHat over I2C.
 *    It's capable of measuring temperature, athmosperic pressure and
 *    relative humidity.
 *****************************************************************************
 *  This file is part of Fågelmataren, an advanced bird feeder equipped with
 *  many peripherals. See <https://github.com/Linkaan/Fagelmatare>
 *  Copyright (C) 2015-2016 Linus Styrén
 *
 *  Fågelmataren is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the Licence, or
 *  (at your option) any later version.
 *
 *  Fågelmataren is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public Licence for more details.
 *
 *  You should have received a copy of the GNU General Public Licence
 *  along with Fågelmataren.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************
 */

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

/*
 *  fagelmatare.ino
 *    Program running on an ATMega328-PU to handle requests and send events
 *    Copyright (C) 2015 Linus Styrén
 *****************************************************************************
 *  This file is part of Fågelmataren:
 *    https://github.com/Linkaan/Fagelmatare/
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *****************************************************************************
 */

#define PING_ENABLED
#include <NewPing.h>

#define MAX_TIME  99999.0f

#define TRIGGER_PIN     10 // Arduino pin tied to trigger pin on ping sensor.
#define ECHO_PIN         9 // Arduino pin tied to echo pin on ping sensor.
#define MAX_DISTANCE   200 // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.

#define COOLDOWN 1000L

#define R2  72000.0f
#define Vin 3.3f

/*
 * These are the coefficients of Steinhart–Hart required to calculate
 * the temperature based on the resistance of my NTC resistor. They have
 * been cut down to ten millionths.
 */
#define A 0.0005651f
#define B 0.0002413f
#define C 0.0000000f

#define SERVO_PIN        8
#define SERVO_BEGIN_POS 180
#define SERVO_END_POS   50
#define SERVO_SPEED     10

#define PING_SPEED      50 // How frequently are we going to send out a ping (in milliseconds). 50ms would be 20 times a second.

#if defined(PING_ENABLED)
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); // NewPing setup of pins and maximum distance.

unsigned long pingtimer;        // Holds the next ping time.
unsigned long cooldown;         // Limit how frequently we rise a moton event.

unsigned int iterations = 5;
float g = 0.99f; // this is a coefficient between 0.0 and 1.0
                // the higher it is the more "inert" the filter will be
float avg_time = MAX_TIME;
float avg_dt = 0.0f;
float dt_hysteresis = 1.0f;
#endif
int duration = 7.222222*SERVO_BEGIN_POS+900;

/*
 * In the setup function we initialize the serial bus and prepare the servo.
 */
void setup() {
  Serial.begin(9600);
#ifndef SERVO_ENABLED
  pinMode(SERVO_PIN, OUTPUT);
#else
  servo.attach(SERVO_PIN);
#endif
#if defined(PING_ENABLED)
  pingtimer = millis(); // Start ping timer now.
#endif
}

/*
 * In order to calculate the median we need a function to sort an array.
 */
void sort_array(float *ar, int n) {
  if (n < 2)
    return;
  float p = ar[n >> 1];
  float *l = ar;
  float *r = ar + n - 1;
  while (l <= r) {
    if (*l < p) {
      l++;
    }
    else if (*r > p) {
      r--;
    }
    else {
      float t = *l;
      *l = *r;
      *r = t;
      l++;
      r--;
    }
  }
  sort_array(ar, r - ar + 1);
  sort_array(l, ar + n - l);
}

/*
 * We use Steinhart–Hart equation to calculate the temperature.
 */
float calculate_temperature(float read_value) {
    /*
     * In our voltage divider we have another resistor where the resistance is known,
     * so we can go back and calculcate the resistance of the sensor.
     */
    float logR1 = log((R2 * read_value)/(1023.0f - read_value));

    return 1.0f / (A + B*logR1 + C*pow(logR1, 3)) - 273.15f; // Use the Steinhart–Hart equation to calculate the temperature.
}

/*
 * We fetch five samples and calculate the median to eleminate noise from sensor readings.
 */
float measure_median_temperature() {
  static const int samples = 5;
  float temperatures[samples];

  /*
   * Obtain five samples using the function above.
   */
  for(int i=0;i<samples;i++) {
    temperatures[i] = analogRead(0); // Fetch the analog read value in the range of 0-1023
    delay(50);
  }

  /*
   * Sort the array with quicksort algorithm.
   */
  sort_array(temperatures, samples);
  return calculate_temperature(temperatures[2]);
}

/*
 * We want to sweep the servo back and forth between SERVO_BEGIN_POS and SERVO_BEGIN_POS.
 */
void sweep() {
  int pos;

  /*
   * Go from SERVO_BEGIN_POS to SERVO_END_POS at speed SERVO_SPEED.
   */
  for(pos=SERVO_BEGIN_POS;pos>=(SERVO_END_POS+1);pos--) {
#ifndef SERVO_ENABLED
    servoMove(pos);
#else
    servo.write(pos);
#endif
    delay(SERVO_SPEED);
  }

  /*
   * Go back from SERVO_END_POS to SERVO_BEGIN_POS at speed SERVO_SPEED.
   */
  for(;pos<SERVO_BEGIN_POS;pos++) {
#ifndef SERVO_ENABLED
    servoMove(pos);
#else
    servo.write(pos);
#endif
    delay(SERVO_SPEED);
  }
}

#ifndef SERVO_ENABLED
void servoMove(int angle) {
  cli();
  duration = 7.222222*angle+900;
  digitalWrite(SERVO_PIN, HIGH);
  delayMicroseconds(duration);
  digitalWrite(SERVO_PIN, LOW);
  sei();
}
#endif

#if defined(PING_ENABLED)
void motion_check() { // Timer2 interrupt calls this function every 24uS where you can check the ping status.
  // Don't do anything here!
  uint8_t rc = sonar.check_timer();
  if(rc==0) { // This is how you check to see if the ping was received.
    float echo_time = (float) sonar.ping_result;

    if(avg_time == MAX_TIME) {
      avg_time = echo_time;
      avg_dt = 0.0f;
    }else {
      float prev_avg_time = avg_time;
      avg_time = avg_time * g + (1.0f - g) * echo_time;
      avg_dt = avg_dt * g + (1.0f - g) * (avg_time - prev_avg_time);

      if(millis() > cooldown && (avg_dt < -dt_hysteresis || avg_dt > +dt_hysteresis)) {
        cooldown = millis() + COOLDOWN;
        Serial.print("/E/");
        Serial.print("motion");
        Serial.print('\0');
      }
    }

    iterations = 5;
  }else if(rc > 1 && --iterations == 0) {
    avg_time = MAX_TIME;
    iterations = 5;
  }
  // Don't do anything here!
}
#endif

/*
 * We use the loop function to check if there are any requests and also to check
 * if the variance of the sensor readings exceeds THRESHOLD.
 */
void loop() {
#if defined(PING_ENABLED)
  if(millis() >= pingtimer) {    // pingSpeed milliseconds since last ping, do another ping.
    pingtimer += PING_SPEED;      // Set the next ping time.
    sonar.ping_timer(motion_check); // Send out the ping, calls "motion_check" function every 24uS where you can check the ping status.
    cli();
    digitalWrite(SERVO_PIN, HIGH);
    delayMicroseconds(duration);
    digitalWrite(SERVO_PIN, LOW);
    sei();
  }
#endif
  if(Serial.available() > 0) {
    String str = Serial.readStringUntil('\0');
    if(str == "servo") {
      sweep();
      return;
    }

    if(str == "temperature") {
      float temperature = measure_median_temperature();
      Serial.print("/R/");
      Serial.print(round(temperature*10));
      Serial.print('\0');
      return;
    }
#if defined(PING_ENABLED)
    if(str == "distance") {
      Serial.print("/R/");
      Serial.print(round(sonar.convert_cm(sonar.ping_median())*10));
      Serial.print('\0');
      return;
    }
#endif
  }
}

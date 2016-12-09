# Fagelmatare Embedded Application

==================================
[![Fagelmatare GPLv3 License](https://img.shields.io/badge/licens-GPLv3_License-blue.svg)](LICENSE)  
An embedded project created to learn Linux and C. Feeds and records birds using a PIR sensor. This is version 2 of this project, take a look at `legacy` branch to see the original project.

Copyright (C) 2016 Linus Styrén <linus122xbb@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

## Prerequisites:

The following hardware is required to use this software:

* 3 Raspberry Pis (Can be any version of RPi, but for best results use a RPi2 or RPi3 as the database and web server and one as the master)  
My configuration is:
 - RPi 3 for database
 - RPi 2 as master
 - RPi 0 as slave
* A decently powerful linux server used to transcode the video streamed from the RPi 2 if you want to run this 24/7  
The linux server is only necessary if you wish to stream to an online service such as ustream.tv
* RPi SenseHat
* RPi Camera Module
* USB Microphone
For best results use a Omni microphone such as this [https://www.amazon.com/CAD-Audio-U9-Condenser-Microphone/dp/B004P1BQG2](U9 Mini Microphone)
* ATMega328-PU
* A waterproof temperature sensor
I use a DS18B20 and built a voltage divider to measure the resistance and calculate the temperature
* Wires, components, breadboards and such that will be necessary to connect everything


## Installation:

In the future an SD card image will be available to download

To build the software yourself, see [INSTALL.md](INSTALL.md).

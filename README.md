### Fågelmatare

### About Fågelmataren

Fågelmataren is an advanced bird feeder equipped with many peripherals to interact with wild birds. This github repository is a collection of programs to achieve the functions below.

Fågelmataren is mainly controlled on a Raspberry Pi and an ATMega328-PU, however to transcode the video from picam library it also uses a quad core laptop. The laptop is also used as a data storage unit for the temperature readings and the P.I.R. sensor readings. Please refer to the flowchart below for further information.

### Functions

- Record H.264/AAC encoded video from Raspberry Pi Camera Module and U9 Mini-mic using picam library
- Livestream to ustream.tv using picam and ffmpeg libraries
- Regularly feed birds at a specified time
- Start recording video when there is a bird pursuant to a P.I.R. sensor
- Regularly log temperature readings from an NTC-resistor via an ATMega328-PU microcontroller

### Programs

- **Fagelmatare-Core** is as the name implies the core program running on the Raspberry Pi to handle P.I.R. sensor interrupts using wiringPi library. The program also communicates via UNIX sockets with the Serial Handler program to handle any events on the ATMega328-PU.
- **Serial Handler** is a program running on the Raspberry Pi to receive events from the ATMega328-PU and dispatch them to any programs subscribed to those events via UNIX sockets. It is also used to send requests and receive an answer which will be sent back to the peer program. It uses wiringPi to send serial messages on the UART interface.
- **Fagelmatare-AVR** is the program running on the ATMega328-PU to handle requests sent from Serial Handler. The program will on demand read the temperature, sweep a servo or fetch the distance to any obstacle in front. It will send an event to Serial Handler whenever there is something obstructing Fågelmataren (e.g a bird).
- **MySQL-Logger** is a library running on the Raspberry Pi used by several other programs to send information/error logs to the Fågelmatare Server.
- **Temperature Handler** is a tiny program running on the Raspberry Pi to request temperature readings via Serial Handler and CPU temperature readings from the sysfs classes.
- **Send Serial** is a small utility program to interact with Serial Handler to send and receive arbitrary messages.

### Workflow
![flowchart](http://i.imgur.com/AM1Va8y.png)

### Thanks to
- Audio/Video recorder for Raspberry Pi: [picam](https://github.com/iizukanao/picam)
- Multimedia framework library: [ffmpeg](https://github.com/FFmpeg/FFmpeg)
- Lock-free Stack codebase: [lstack](https://github.com/skeeto/lstack)
- Arduino wiring function library: [wiringPi](https://github.com/WiringPi/WiringPi)

### License
The codebase for Fågelmataren is LPGL-licensed. Please refer to the LICENSE file for detailed information.

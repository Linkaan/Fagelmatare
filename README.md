### Fågelmatare

[![Join the chat at https://gitter.im/Linkaan/Fagelmatare](https://badges.gitter.im/Linkaan/Fagelmatare.svg)](https://gitter.im/Linkaan/Fagelmatare?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

### About Fågelmataren

Fågelmataren is an advanced bird feeder equipped with many peripherals to interact with and "spy" on wild birds. This github repository is a collection of programs used on Fågelmataren itself.

Fågelmataren is mostly self-contained and it is controlled from a Raspberry Pi 2. The master Raspberry Pi uses a slave Raspberry Pi Zero with a SenseHat for logging temperature, atmospheric pressure and relative humidity. To feed the birds a valve is opened at a fixed time every day. This valve is controlled by a servo. To steer this servo an ATMega328-PU is used. The ATMega328-PU is also used to log the temperature outside of the box.

Apart from the stand-alone unit which I refer to as Fågelmataren, a server is used to host a database to store the sensor measurements and videos recorded. It is also necessary to transcode the video stream from Fågelmataren to a lower quality stream with lower bitrate and resolution before streaming it to ustream. The server is running partialy on a quadcore laptop and a Raspberry Pi 3 but I am planning to replace it fully with a Raspberry Pi 3. For more details on how Fågelmataren works please refer to the flowchart below.

### Functions

- Record H.264/AAC encoded video from Raspberry Pi Camera Module and U9 Mini-mic using picam library
- Livestream to ustream.tv using picam and ffmpeg
- Feed birds at a fixed time every day
- Intelligent motion detection system used to start a recording whenever there is a motion event
- Log outside temperature measurements from an thermistor using an ATMega328-PU microcontroller
- Log inside temperature (inside of the box), atmospheric pressure and relative humidity using the
  Raspberry Pi SenseHat
- Nightvision using an IR illuminator and a IR cut-filter shutter.
- Record timelapse videos

### Programs

- **Fagelmatare-Core** is as the name implies the core program running on the Raspberry Pi 2. The main purpose of the program is to handle events and take actions and also control picam. It communicates via Serial Handler to retrieve events from the slave Raspberry Pi and the ATMega328-PU microcontroller. When a motion event is issued it will use picam to start recording. If no motion event has been detected within a timeframe of 5 seconds and picam is recording, the recording will be stopped.
- **Serial Handler** runs in parallel with Fagelmatare-Core on the Raspberry Pi 2. The program is essentially an event handler. The program is built to have a subscription system to which any parallel programs (such as Fagelmatare-Core) can subscribe to particular events via IPC. The subscription system also accepts subscriptions via TCP. Serial Handler handles any incoming events, for example from the ATMega328-PU, and dispatches them to any programs subscribed to those particular events. It can also be used to send requests and receive a response which will be sent back via the same connection which issued the request. In the first version of the program all it did was use wiringPi to communicate via the UART bus on the raspberry pi with the ATMega328-PU microcontroller hence the name Serial Handler.
- **Fagelmatare-Zero** is the program running on the slave Raspberry Pi. Its main purpose is to log sensor measurements from the SenseHat but also from the thermistor and CPU temperature on Raspberry Pi 2. It is subscribed over a TCP connection to Serial Handler to a particular event which will be risen by Temperature Handler. It handles the events sent from the ATMega328-PU.
- **Fagelmatare-AVR** is the program running on the ATMega328-PU to handle requests sent from Serial Handler and send back a response. The program will on demand read the outside temperature from a thermistor, open the valve using a servo. It will send an event to Serial Handler whenever there is something obstructing Fågelmatarens view (e.g a bird). It uses an ultrasonic sensor to do this.
- **MySQL-Logger** is a library running used by Fagelmatare-Core, Serial Handler and Fagelmatare-Zero to log information/error logs to the database on the server.
- **Temperature Handler** is a tiny program running on the Raspberry Pi 2 to rise an event which will be handled on the Raspberry Pi Zero. When the event is handled it will log sensor measurements from the SenseHat, temperature from the thermistor connected to the ATMega328-PU and the CPU temperature (on the Raspberry Pi 2).
- **Send Serial** is a small utility program to interact with Serial Handler to send and receive arbitrary messages.
- **Send Event** is a small utility program to interact with Serial Handler to rise arbitrary events.

### Workflow
![flowchart](http://i.imgur.com/UwN4aM8.png)

### Thanks to
- Audio/Video recorder for Raspberry Pi: [picam](https://github.com/iizukanao/picam)
- Multimedia framework library: [ffmpeg](https://github.com/FFmpeg/FFmpeg)
- Lock-free Stack codebase: [lstack](https://github.com/skeeto/lstack)
- Raspberry Pi GPIO access library: [wiringPi](https://projects.drogon.net/raspberry-pi/wiringpi/)
- Instrument control, data acquisition display and analysis: [experix laboratory control system](https://sourceforge.net/projects/experix/)

### License
The codebase for Fågelmataren is GPL-licensed. Please refer to the LICENSE file for detailed information.

# Installation Guide

This guide describes how to install the necessary software for this project. This guide is written for Linux-based systems.

## Installing tiny core linux

Tiny Core Linux is used as the toolkit to create our customized embedded system to run our software.

### Master RPi

Get the latest version of picore for your RPi. If you use a RPi 1 as the master you should download from <http://tinycorelinux.net/8.x/armv6/releases/RPi/> instead. Same goes for all the future dependencies we download from <http://tinycorelinux.net>.
```bash
$ cd ~
$ wget http://tinycorelinux.net/8.x/armv7/releases/RPi/piCore-8.0.zip
$ unzip piCore-8.0.zip
```
Identify location of sd card by first running `lsblk` and then insert sd card and run `lsblk`. The sd card should show up as a device which is not present the first time you ran `lsblk`.

Now we install piCore with dd command (replace /dev/sdx with the location of your sd card)
```bash
$ sudo dd if="~/piCore-8.0.img" of="/dev/sdx"
$ sync
```
Plug in an ethernet cable to your master RPi, insert the sd card and boot it up. If you have your RPi connected to a monitor with a keyboard run `ifconfig eth0` to get the IP address which will be used to login remotely over SSH. Otherwise if the RPi is on the same LAN as your Linux box you can run nmap to scan for the RPi with this command (use `ifconfig` on your machine if you are not sure if your network is 192.168.1.0):
```bash
$ nmap -v -sn 192.168.1.0/24
```
An alternative is to use Fing on your mobile phone which can be used to grab the IP of the RPi.
Now run this command on your machine to remotely connect to your RPi:
```bash
$ ssh tc@<replace with IP of RPi>
```
The default password is "piCore"

Once you're logged in we need to partition our SD card to add persistent storage. We need this to install our software. We want to expand the mmcblk0p2 partition to fill our SD card.

1. Start fdisk partitiong tool as root:
   ```bash
   tc@box:~$ sudo fdisk -u /dev/mmcblk0
   ```
   Now list partitions with 'p' command and write down the starting and ending  sectors of the second partition.
 2. Delete second partition with `d` then recreate it with `n` command.
   Use the same starting sector as deleted had and provide end sectore or size greater than deleted had having enough free space for Mounted Mode. When finished, exit fdisk with 'w' command. Now the partition size increased but file system size is not yet changed.
 3. Reboot piCore. It is necessary to make Kernel aware of changes.
 4. After reboot expand file system to the new partition boundaries with typing  the following command as root:
    ```bash
    tc@box:~$ sudo resize2fs /dev/mmcblk0p2
    ```
### Slave RPi

Do the same procedure for the slave as for the master but remember to download from the correct architecture. It will be armv6 if you're using a RPi model A, B, A+, B+ or zero) and armv7 for the second generation of RPi.

## Configuring our master

SSH into your master RPi (procedure to retrieve IP of RPi is described above):
```bash
$ ssh tc@<replace with IP of RPi>
```

### Enabling camera

We need to enable the camera on our master RPi. We must modify our config.txt file so first we mount the boot partition and then open our config.txt file:
```bash
tc@box:~$ sudo mkdir /boot
tc@box:~$ sudo mount -t vfat /dev/mmcblk0p1 /boot/
tc@box:~$ sudo vi /boot/config.txt
```
Now we need to add the following parameters under the "[ALL]" section:
```
gpu_mem=128
start_file=start_x.elf
fixup_file=fixup_x.dat
```
Basic usage of `vi` if you're not used to it: press i to enter edit mode, then add the content and press esc to go back to command mode and type `:wq` to write the changes and exit. Same procedure when you use `vi` hereafter.

We also need to allow all users to access /dev/vchiq to use the camera. So we need to chmod of it everytime we boot by adding it to /opt/bootlocal.sh
```bash
tc@box:~$ sudo vi /opt/bootlocal.sh
```
Append the following line to the file
```
chmod 777 /dev/vchiq
```
### Installing dependencies

We need to make a backup of the filesystem to keep our changes after the system shuts down. But first we want to install a few dependencies that we will use in our software later. We want to install **libasound-dev.tcz**, **fontconfig-dev.tcz**, **harfbuzz-dev.tcz**, **openssl-dev.tcz** and **rsync.tcz**.
```bash
tc@box:~$ tce-load -wi libasound-dev.tcz
tc@box:~$ tce-load -wi harfbuzz-dev.tcz
tc@box:~$ tce-load -wi fontconfig-dev.tcz
tc@box:~$ tce-load -wi openssl-dev.tcz
tc@box:~$ tce-load -wi rsync.tcz
```
Now we have installed the dependencies for our software. Let's backup the filesystem and reboot.
```bash
tc@box:~$ sudo filetool.sh -b
tc@box:~$ sudo reboot
```

### Building and configuring crosstool-ng

We want to setup our cross compiler to be able to compile the software for our specific RPi architecture. We need to install a few packages that are required to build crosstool-ng which is the cross compiler that we are going to use. This procedure is done on your main Linux machine.

When we try to configure the cross compiler, it should inform about any missing dependencies that you need to install. On debian based systems we start by installing the following packages:
```bash
$ sudo apt-get update
$ sudo apt-get install flex bison automake gperf libtool libtool-bin patch texinfo ncurses-dev help2man
```
We create a new directory for our master toolchain.
```bash
$ mkdir ~/master_toolchain
$ cd ~/master_toolchain
```
Download the latest release version of crosstool-ng from <http://crosstool-ng.org/download/crosstool-ng/>. **NOTE** the current version available as of right now (1.22.0) doesn't work because of 404 errors when trying to build crosstool-ng. If there is a more recent version available than 1.22.0 try that otherwise we have to download from the development branch by cloning the git repository:
```bash
$ git clone https://github.com/crosstool-ng/crosstool-ng
```
We also need to generate our configure file if we downloaded the development branch:
```bash
$ cd crosstool-ng
$ ./bootstrap
```
Otherwise if you downloaded the release version just untar the tar.xz archive.
```bash
$ tar xvf crosstool-ng-*.tar.xz
$ cd crosstool-ng
```

Next whether we downloaded crosstool-ng from the development branch or the latest working release we want to configure our toolchain:
```bash
$ ./configure --prefix=$HOME/master_toolchain/cross
```
If configure completes without any errors we can compile it, otherwise install any dependencies it says are missing.
```bash
$ make
$ sudo make install
```
Ok, now we need to configure the cross compiler to be able to compile for our RPi architecture.
```bash
$ export PATH=$PATH:$HOME/master_toolchain/cross/bin
$ cd ~/master_toolchain
$ mkdir ctng
$ cd ctng
$ ct-ng menuconfig
```
Since the architecture for RPi generation 1 and generation 2 are different we need to configure crosstool-ng for our specific RPi.
#### Configuration for RPi gen 2

- Paths and misc options
    - Check "Try features marked as EXPERIMENTAL"
    - Set "Prefix directory" to "${HOME}/master_toolchain/cross/x-tools/${CT_TARGET}"
    - (OPTIONAL) Set "Number of parallel jobs" to amount of cores your processor has
- Target options
    - Set "Target Architecture" to "arm"
    - Set "Endianness" to "Little endian"
    - Set "Bitness" to "32-bit"
    - Set "Emit assembly for CPU" to "cortex-a7"
    - Set "Use specific FPU" to "neon-vfpv4"
    - Set "Floating point" to "hardware (FPU)"
    - Set "Default instruction set mode" to "arm"
    - Check "Use EABI"
- Toolchain options
    - Set "Tuple's vendor string" to "rpi"
- Operating System
    - Set "Target OS" to "linux"
- Binary utilities
    - Set "Binary format" to "ELF"
    - Set "binutils version" to "2.26"
- C-library
    - Set "C library" to "glibc"
    - Set "glibc version" to "2.24"
- C compiler
    - Check "Show Linaro versions"
    - Set "gcc version" to "linaro-5.2-2015.11-2"
    - Set "gcc extra config" to "--with-float=hard"
    - Check "Link libstdc++ statically into the gcc binary"
    - Check "C++" under "Additional supported languages"

#### Configuration for RPi gen 1

- Paths and misc options
    - Check "Try features marked as EXPERIMENTAL"
    - Set "Prefix directory" to "${HOME}/master_toolchain/cross/x-tools/${CT_TARGET}"
    - (OPTIONAL) Set "Number of parallel jobs" to amount of cores your processor has
- Target options
    - Set "Target Architecture" to "arm"
    - Set "Endianness" to "Little endian"
    - Set "Bitness" to "32-bit"
    - Set "Emit assembly for CPU" to "arm1176jzf-s"
    - Set "Use specific FPU" to "vfp"
    - Set "Floating point" to "hardware (FPU)"
    - Set "Default instruction set mode" to "arm"
    - Check "Use EABI"
- Toolchain options
    - Set "Tuple's vendor string" to "rpi"
- Operating System
    - Set "Target OS" to "linux"
- Binary utilities
    - Set "Binary format" to "ELF"
    - Set "binutils version" to "2.26"
- C-library
    - Set "C library" to "glibc"
    - Set "glibc version" to "2.24"
- C compiler
    - Check "Show Linaro versions"
    - Set "gcc version" to "linaro-5.2-2015.11-2"
    - Set "gcc extra config" to "--with-float=hard"
    - Check "Link libstdc++ statically into the gcc binary"
    - Check "C++" under "Additional supported languages"

Now we save our configration and build crosstool-ng with the settings we chose.
```bash
$ sudo chown -R $(whoami) $HOME/master_toolchain/cross
$ ct-ng build
```

### Testing our cross compiler

Before we start building our main software, we want to do a quick sanity check of our ARM compiler. We export CCPREFIX to be able to compile and add our prefix directory to the path:
```bash
$ export PATH=$PATH:$HOME/master_toolchain/cross/x-tools/arm-rpi-linux-gnueabihf/bin
$ export CCPREFIX="$HOME/master_toolchain/cross/x-tools/arm-rpi-linux-gnueabihf/bin/arm-rpi-linux-gnueabihf-"
```
If everything is setup correctly you should be able to get the current version of the ARM compiler:
```bash
$ arm-rpi-linux-gnueabihf-gcc --version
```
Now we try to build a simple hello world program in C. Open test.c in your favourite editor (vim, nano, emacs) and add the following content:
```c
#include <stdio.h>

int main() {
    printf("Hello, world!\n");
    return 0;
}
```
Now we try building our "test" binary and copying it to the master RPi.
```bash
$ arm-rpi-linux-gnueabihf-gcc -o test test.c
$ rsync -rav test tc@<replace with IP of RPi>:test
```
SSH into the RPi with password "piCore" and run the program:
```bash
$ ssh tc@<replace with IP of RPi>
tc@box:~$ sudo chmod +x test
tc@box:~$ ./test
Hello, world!
```
If the program works it will display "Hello, world!" and you're ready to compile our main software. If it doesn't work make sure you configured the cross compiler correctly.

### Compiling ffmpeg and picam

All binaries and header files will go into the `~/master_toolchain/pi/build` directory that we must create. We will also save it into a environment variable called `$PIBUILD`:
```bash
$ export PIBUILD=$HOME/master_toolchain/pi/build
$ mkdir -p $PIBUILD
$ cd ~/master_toolchain/pi
```

### Building fdk-aac

Download the latest version of fdk-aac from <http://sourceforge.net/projects/opencore-amr/>. Then let's unpack the source code:
```bash
$ tar zxvf fdk-aac-*.tar.gz
$ cd fdk-aac-*
```
Next we need to configure fdk-aac to use our cross compiler and to put the files into our `$PIBUILD` directory. Then we can build fdk-aac:
```bash
$ CC=${CCPREFIX}gcc CXX=${CCPREFIX}g++ ./configure --host=arm-rpi-linux-gnueabi --prefix=$PIBUILD
$ make
$ make install
```
### Copying dependencies

We need to copy all the required dependencies from our RPi so that we can build ffmpeg. Let's put this content in the `~/master_toolchain/pi/usr` directory that we must create along with subdirectories `include` and `lib`:
```bash
$ export PIUSR=$HOME/master_toolchain/pi/usr
$ mkdir $PIUSR
$ cd $PIUSR
$ mkdir include lib
```
Now we can copy all the RPi libraries that are required to build our main software, ffmpeg and picam. We use rsync for this (password is `piCore`):
```bash
$ rsync -ravh -L tc@<replace with IP of RPi>:/usr/local/include/ $PIUSR/include/
$ rsync -ravh -L tc@<replace with IP of RPi>:/usr/local/lib/ $PIUSR/lib/
$ rsync -ravh -L tc@<replace with IP of RPi>:/usr/lib/ $PIUSR/lib/
```

### Building ffmpeg

Now we can clone and build ffmpeg and configure it to use fdk-aac library and alsa:
```bash
$ cd ~/master_toolchain/pi
$ git clone git://source.ffmpeg.org/ffmpeg.git
$ cd ffmpeg
$ PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$PIBUILD/lib/pkgconfig CC=${CCPREFIX}gcc CXX=${CCPREFIX}g++ ./configure --enable-cross-compile --cross-prefix=${CCPREFIX} --arch=armel --target-os=linux --prefix=$PIBUILD --extra-cflags="-I$PIBUILD/include -I$PIUSR/include -fPIC" --extra-ldflags="-L$PIBUILD/lib -L$PIUSR/lib -fPIC" --enable-libfdk-aac --pkg-config=`which pkg-config`
```
Make sure that "Enabled indevs" has "alsa" and "Enabled encoders" has "libfdk_aac". If not make sure you followed all the steps correctly. Next we want to build ffmpeg with the same amount of parallell jobs as we have cores on our machine. Type the following command to get the amount of cores on your machine if you don't know:
```bash
$ cat /proc/cpuinfo | grep processor | wc -l
```
Next we build ffmpeg:
```bash
$ make -j <num cores>
$ make install
```

### Downloading RPi firmware

To be able to build libilclient we need to download the source code for the RPi firmware:
```bash
$ cd ~/master_toolchain/pi
$ git clone https://github.com/raspberrypi/firmware
```
We want to copy the content of `opt/vc/` into the directory `~/master_toolchain/pi/opt/vc/`:
```bash
$ export PIOPT=$HOME/master_toolchain/pi/opt/vc
$ mkdir -p $PIOPT
$ rsync -rav ~/master_toolchain/firmware/opt/vc/ $PIOPT/
```

### Building libilclient

Because libilclient is normally built on the RPi itself, we need to change the Makefile to use our paths. With a bit of magic we can use `sed` to do this:
```bash
$ sed -i 's/\opt\//${OPT}\//g' $PIOPT/src/hello_pi/Makefile.include
```
Now we can simply set the environment variable OPT to the path of our `opt` directory and build ilclient:
```bash
$ cd $PIOPT/src/hello_pi/libs/ilclient
$ CC=${CCPREFIX}gcc OPT=$HOME/master_toolchain/pi/opt make
```

### Building picam

Now let's download the source code for picam from github so that we can build it:
```bash
$ cd ~/master_toolchain/pi
$ git clone https://github.com/iizukanao/picam
$ cd picam
```
Picam is normally built on the RPi so as we did above with `libilclient` we need to modify the Makefile for picam. This time it is easier to open the Makefile in an editor. We need to do the following changes:
```diff
-1 CC
-2 CFLAGS=-DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -fPIC -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -Wall -g -DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT -ftree-vectorize -DUSE_EXTERNAL_OMX -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -Wno-psabi -I/opt/vc/include/ -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/src/hello_pi/libs/ilclient `freetype-config --cflags` `pkg-config --cflags harfbuzz fontconfig libavformat libavcodec` -I/usr/include/fontconfig -g -Wno-deprecated-declarations -O3
+2 CFLAGS=-DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -fPIC -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -Wall -g -DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT -ftree-vectorize -DUSE_EXTERNAL_OMX -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -Wno-psabi -I${OPT}/vc/include/ -I${OPT}/vc/include/interface/vcos/pthreads -I${OPT}/vc/include/interface/vmcs_host/linux -I${OPT}/vc/src/hello_pi/libs/ilclient -I${USR}/include/freetype2 -I${USR}/include/libpng16 -I${USR}/include/harfbuzz -I${USR}/include/glib-2.0 -I${USR}/lib/glib-2.0/include -I${USR}/include -I${USR}/include/fontconfig -I${PIBUILD}/include -g -Wno-deprecated-declarations -O3
-3 LDFLAGS=-g -Wl,--whole-archive -lilclient -L/opt/vc/lib/ -L/usr/local/lib -lGLESv2 -lEGL -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -lpthread -lrt -L/opt/vc/src/hello_pi/libs/ilclient -Wl,--no-whole-archive -rdynamic -lm -lcrypto -lasound `freetype-config --libs` `pkg-config --libs harfbuzz fontconfig libavformat libavcodec`
+3 LDFLAGS=-g -Wl,--whole-archive -lilclient -L${PIBUILD}/lib -L${OPT}/vc/lib/ -L${USR}/lib -lGLESv2 -lbrcmGLESv2 -lEGL -lbrcmEGL -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -lglib-2.0 -lpcre -lbz2 -lgraphite2 -lexpat -lpthread -lrt -L${OPT}/vc/src/hello_pi/libs/ilclient -Wl,--no-whole-archive -rdynamic -lm -lcrypto -lasound -lfreetype -lz -lpng -lharfbuzz -lfontconfig -lavformat -lavcodec -lfdk-aac -lswscale -lavutil -lavfilter -lswresample -lavdevice
-4 DEP_LIBS=/opt/vc/src/hello_pi/libs/ilclient/libilclient.a
+4 DEP_LIBS=${OPT}/vc/src/hello_pi/libs/ilclient/libilclient.a
-9 RASPBERRYPI=$(shell sh ./whichpi)
-10 GCCVERSION=$(shell gcc --version | grep ^gcc | sed "s/.* //g")
-12 # detect if we are compiling for RPi 1 or RPi 2 (or 3)
-13 ifeq ($(RASPBERRYPI),Pi1)
-14         CFLAGS += -mfpu=vfp -mfloat-abi=hard -march=armv6zk -mtune=arm1176jzf-s
-15 else
-16 ifneq (,$(findstring 4.6.,$(GCCVERSION)))  # gcc version 4.6.x
-17         CFLAGS += -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7-a
-18 else  # other gcc versions
-19         CFLAGS += -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7-a -mtune=cortex-a7
-20 endif
-21 endif
```
Now we should be able to build picam:
```bash
$ CC=${CCPREFIX}gcc OPT=$HOME/master_toolchain/pi/opt USR=$HOME/master_toolchain/pi/usr PIBUILD=$PIBUILD make
```
If everything went well, we can strip the binary to save some disk space:
```bash
$ strip picam
```

### Packaging picam into a tcz package

We must add our libraries into a common package that can be installed on our RPi. A tcz package is a in reality a squashfs, so we can simply create a directory that corresponds to the path that we want the files to be put into on our real filesystem when the raspberry pi reboots. Let's put in all the videocore libraries:
```bash
$ export PILIB=$HOME/master_toolchain/pi/picam/squashfs/usr/local/lib
$ mkdir -p $PILIB
$ rsync -ravh $PIOPT/lib/ $PILIB/
```
We also need the libav libraries and libfdk-aac:
```bash
$ rsync -ravh $PIBUILD/lib/ $PILIB/
```
Because we want to use the timestamp and subtitle functions that picam offers we need to install a font to use. You can download any font as long as it is of the type `ttf` (I recommend to use the liberation font-family):
```bash
$ mkdir -p $PILIB/../fonts
$ cd $PILIB/../fonts
$ wget https://fedorahosted.org/releases/l/i/liberation-fonts/liberation-fonts-ttf-2.00.1.tar.gz
$ tar zxvf liberation-fonts-ttf-2.00.1.tar.gz
```
Lastly we also need to copy our picam binary to a bin folder:
```bash
$ mkdir -p $PILIB/../bin
$ cp ~/master_toolchain/pi/picam/picam $PILIB/../bin
```
Now to actually make a squashfs file we need to have the `squashfs-tools` package installed. To install it with aptitude package manager:
```bash
$ sudo apt-get install squashfs-tools
```
Then we can make our `picam.tcz` package by running:
```bash
$ mksquashfs $HOME/master_toolchain/pi/picam/squashfs/ picam.tcz
```

### Testing picam.tcz

Before we install the picam package we will test that it works. Let's copy it over to our RPi using rsync:
```bash
$ rsync -ravh $HOME/master_toolchain/pi/picam/picam.tcz tc@<replace with IP of RPi>:picam.tcz
```
Now SSH into the RPi and we will load the tcz package:
```bash
$ ssh tc@<replace with IP of RPi>
tc@box:~$ tce-load -i picam.tcz
```
Now we will try to run picam:
```bash
tc@box:~$ /usr/local/bin/picam --time &
```
If picam reports it can't initialize the audio, you can list all the microphones and find the alsa device name:
```bash
tc@box:~$ tce-load -wi alsa-utils.tcz
tc@box:~$ arecord -l
**** List of CAPTURE Hardware Devices ****
card 1: Device [USB PnP Sound Device], device 0: USB Audio [USB Audio]
  Subdevices: 1/1
    Subdevice #0: subdevice #0
```
The device name will look like `hw:<card>,<device>`. So for example in the example above the device name will be `hw:1,0`. Let's restart picam with out correct ALSA device name:
```bash
tc@box:~$ killall picam
tc@box:~$ /usr/local/bin/picam --alsadev hw:1,0 --time &
```
Now to start a recording we will have to create an empty `start_record` file in the `hooks` directory:
```bash
tc@box:~$ touch hooks/start_record
```
You should see `start rec`. Likewise to stop the recording create an empty `stop_record` file in the `hooks` directory:
```bash
tc@box:~$ touch hooks/stop_record
tc@box:~$ killall picam
tc@box:~$ exit
```
Now back on your main Linux box you can copy the video using rsync:
```bash
$ rsync -ravh tc@<replace with IP of RPi>:rec/archive/*.ts ~/videos/
```
Goto the `~/videos/` directory on your computer and you should be able to see a video file there. Open it with `vlc` or any other video player that supports playing `ts` files. If the video and audio seems good then we can move on to installing picam on our system permanently.

### Installing picam.tcz

SSH into the RPi and copy the `picam.tcz` package to `/mnt/mmcblk0p2/tce/optional`:
```bash
$ ssh tc@<replace with IP of RPi>
tc@box:~$ cp picam.tcz /mnt/mmcblk0p2/tce/optional/
```
Now to load `picam.tcz` on boot we need to add it to the `onboot.lst` file:
```bash
tc@box:~$ vi /mnt/mmcblk0p2/tce/onboot.lst
```
Now append `picam.tcz` to the file and then we reboot our RPi to see if it worked:
```bash
tc@box:~$ sudo reboot
```
SSH back into your RPi and try running picam to see if it works. If everything works correctly you can move on to setting up the main software on the RPi.

### Compiling main software

# Installation Guide

This guide describes how to install the necessary software for this project. This guide is written for Linux-based systems.

## Installing tiny core linux

Tiny Core Linux is used as the toolkit to create our customized embedded system to run our software.

Get the latest version of picore (piCore-8.1.5 as of now). You can download a universal image from <http://tinycorelinux.net/8.x/armv6/releases/RPi/>:
```bash
$ cd ~
$ wget http://tinycorelinux.net/8.x/armv6/releases/RPi/piCore-8.1.5.zip
$ unzip piCore-8.1.5.zip
```

### Preparing the SD-card

The following procedure is the same for both the master and slave RPi.

Identify location of the sd card by first running `lsblk` and then insert sd card and run `lsblk`. The sd card should show up as a device which is not present the first time you ran `lsblk`.

Now we install piCore with dd command (replace /dev/sdx with the location of your sd card)
```bash
$ sudo dd if="~/piCore-8.1.5.img" of="/dev/sdx"
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
The default password is "piCore".

Once you're logged in we need to partition our SD card to add persistent storage. We need this to install our software. We want to expand the mmcblk0p2 partition to fill our SD card.

1. Start fdisk partitiong tool as root:
   ```bash
   tc@box:~$ sudo fdisk -u /dev/mmcblk0
   ```
   Now list partitions with 'p' command and write down the starting and ending  sectors of the second partition.
2. Delete second partition with `d` then recreate it with `n` command.
   Use the same starting sector as deleted had and provide end sectore or size greater than deleted had having enough free space for Mounted Mode. When finished, exit fdisk with 'w' command. Now the partition size increased but file system size is not yet changed.
3. Reboot piCore. It is necessary to make Kernel aware of changes.
4. After reboot ssh back into fgmaster and expand file system to the new partition boundaries with typing  the following command as root:
    ```bash
    tc@box:~$ sudo resize2fs /dev/mmcblk0p2
    ```

### Slave RPi

First prepare the SD-card for the slave as described above. 

If you are using a RPi zero as the slave you must have a USB ethernet adapter, however it is much easier to just configure the slave on the master RPi so that you can use ethernet.

If you are configuring on a RPi zero you need to also install the `net-usb-4.4.39-piCore+.tcz` package and put that manually on the sd card. If your operating system doesn't automatically mount the sd card for you, you must do it yourself. First identify the location of the sd card by first runnng `lsblk` without the sd card inserted, then insert the sdcard again and run `lsblk`. The sd card should show up as a device which was not present the first time you ran lsblk. Now mount the
second partition to somewhere (of course replace /dev/sdx with actual location of SECOND partition for the sd card):
```bash
$ sudo mkdir -p /mnt/sdcard
$ sudo mnt -t ext4 /dev/sdx2 /mnt/sdcard
```
Now install the net usb modules:
```bash
$ cd /mnt/sdcard/tce/optional
$ wget tinycorelinux.net/8.x/armv6/tcz/net-usb-4.4.39-piCore+.tcz
$ cd ..
$ echo net-usb-4.4.39-piCore+.tcz >> onboot.lst
```

## Configuring our master

We are going to connect to our master and slave several times so it is best to configure a shorthand for it by editing the `~/.ssh/config` file and appending this as the master entry:
```bash
Host fgmaster
  HostName <replace with IP of RPi>
  User tc
```
SSH into your master RPi to see if it works:
```bash
$ ssh fgmaster
tc@box:~$ exit
```

It is annoying to have to type "piCore" as the password everytime so let's authorize our computer to allow to connect without a password and then also change it to a more secure one.

If you haven't got a ssh key setup to use (usually it is the `~/.ssh/id_rsa` file) then we have to generate one (if you have one you can jump directly to [Adding our ssh key to authorized_keys](adding-our-ssh-key-to-authorized_keys) but make sure you have the ssh-agent setup properly if you are using a passphrase for the ssh key):
```bash
$ ssh-keygen -t rsa -b 4096 -C "your_email@example.com"
```
You will be asked where to save the ssh key, save it as `~/.ssh/fgid_rsa`. Set the passphrase to something secure that you will remember. Now you should have a rsa key that can be used to authorize to connect without a password to the master. Let's also start ssh-agent in the background which manages our ssh key and remembers our passphrase so that we don't have to type the passphrase all the time:
```bash
$ eval "$(ssh-agent -s)"
$ ssh-add ~/.ssh/fgid_rsa
```
### Adding our ssh key to authorized_keys

Now copy the ~/.ssh/fgid_rsa.pub or your the corresponding public key that you might already have had before and ssh into master and paste it into `~/.ssh/authorized_keys`:
```bash
$ ssh fgmaster
tc@box:~$ mkdir ~/.ssh
tc@box:~$ touch ~/.ssh/authorized_keys
tc@box:~$ vi ~/.ssh/authorized_keys
```

Basic usage of `vi` if you're not used to it: press i to enter edit mode, then add the content and press esc to go back to command mode and type `:wq` to write the changes and exit. Same procedure when you use `vi` hereafter.

Paste in your public key with CTRL+SHIFT+V when you are in edit mode (insert mode). Now close the ssh connection and try connecting again to make sure things are working:
```bash
tc@box:~$ exit
$ ssh fgmaster
```
Now if you can connect without the password change it to something secure instead of having the default `piCore` password:
```bash
tc@box:~$ passwd
```

### Setting timezone

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
# optionally (to not scare birds):
disable_camera_led=1
```

We also need to allow all users to access /dev/vchiq to use the camera. So we need to chmod of it everytime we boot by adding it to /opt/bootlocal.sh
```bash
tc@box:~$ sudo vi /opt/bootlocal.sh
```
Append the following line to the file
```
chmod 777 /dev/vchiq
```
### Installing dependencies

We need to make a backup of the filesystem to keep our changes after the system shuts down. But first we want to install a few dependencies that we will use in our software later. We want to install **alsa.tcz**, **libasound-dev.tcz**, **fontconfig-dev.tcz**, **harfbuzz-dev.tcz**, **openssl-dev.tcz** and **rsync.tcz**.
```bash
tc@box:~$ tce-load -wi alsa.tcz
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

### Building and configuring crosstool-ng for master

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
    - Set "binutils version" to "2.27"
- C-library
    - Set "C library" to "glibc"
    - Set "glibc version" to "2.25"
- C compiler
    - Check "Show Linaro versions"
    - Set "gcc version" to "linaro-6.2-2016.11"
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
    - Set "binutils version" to "2.27"
- C-library
    - Set "C library" to "glibc"
    - Set "glibc version" to "2.25"
- C compiler
    - Check "Show Linaro versions"
    - Set "gcc version" to "linaro-6.2-2016.11"
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
$ rsync -rav test fgmaster:test
```
SSH into the RPi with password "piCore" and run the program:
```bash
$ ssh fgmaster
tc@box:~$ sudo chmod +x test
tc@box:~$ ./test
Hello, world!
```
If the program works it will display "Hello, world!" and you're ready to compile the main software. If it doesn't work for you make sure you configured the cross compiler correctly.

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
$ rsync -ravh -L fgmaster:/usr/local/include/ $PIUSR/include/
$ rsync -ravh -L fgmaster:/usr/local/lib/ $PIUSR/lib/
$ rsync -ravh -L fgmaster:/usr/lib/ $PIUSR/lib/
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

We must add our libraries into a common package that can be installed on our RPi. A tcz package is in reality a squashfs, so we can simply create a directory that corresponds to the path that we want the files to be put into on our real filesystem when the raspberry pi reboots. Let's put in all the videocore libraries:
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
$ cd ~/master_toolchain/pi/picam
$ mksquashfs squashfs/ picam.tcz
```

### Testing picam.tcz

Before we install the picam package we will test that it works. Let's copy it over to our RPi using rsync:
```bash
$ rsync -ravh $HOME/master_toolchain/pi/picam/picam.tcz fgmaster:picam.tcz
```
Now SSH into the RPi and we will load the tcz package:
```bash
$ ssh tc@fgmaster
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
The device name will look like `hw:<card>,<device>`. So for example in the example above the device name will be `hw:1,0`. Let's restart picam with our correct ALSA device name:
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
$ rsync -ravh fgmaster:rec/archive/*.ts ~/videos/
```
Goto the `~/videos/` directory on your computer and you should be able to see a video file there. Open it with `vlc` or any other video player that supports playing `ts` files. If the video and audio seems good then we can move on to installing picam on our system permanently.

### Installing picam.tcz

SSH into the RPi and copy the `picam.tcz` package to `/mnt/mmcblk0p2/tce/optional`:
```bash
$ ssh fgmaster
tc@box:~$ mv picam.tcz /mnt/mmcblk0p2/tce/optional/
```
Now to load `picam.tcz` on boot we need to append it to the `onboot.lst` file:
```bash
tc@box:~$ echo picam.tcz >> /mnt/mmcblk0p2/tce/onboot.lst
```
Now we reboot our RPi to see if it worked:
```bash
tc@box:~$ sudo reboot
```
SSH back into your RPi and try running picam to see if it works. If everything works correctly you can move on to setting up the main software on the RPi.

### Building wiringPi

The main main software requires to use `wiringPi` so let's download the source code:
```bash
$ cd ~/master_toolchain/pi
$ git clone git://git.drogon.net/wiringPi
```
The reason we can't simply download the `wiringPi.tcz` package from the tiny core linux repository is because we need to patch the `wiringPiISR` function so that it accepts a userdata argument. Let's edit the wiringPi.c file:
```bash
$ vi ~/master_toolchain/wiringPi/wiringPi/wiringPi.c
```
Now go ahead and do the following changes (line numbers might not be exactly the same):
```diff
312:
-static void (*isrFunctions [64])(void) ;
+static void (*isrFunctions [64])(void *) ;
1811:
-isrFunctions [myPin] () ;
+isrFunctions [myPin] (arg) ;
1825:
-int wiringPiISR (int pin, int mode, void (*function)(void))
+int wiringPiISR (int pin, int mode, void (*function)(void *), void *arg)
1907:
-pthread_create (&threadId, NULL, interruptHandler, NULL) ;
+pthread_create (&threadId, NULL, interruptHandler, arg) ;
```
Now let's do some changes to the wiringPi.h file:
```bash
$ vi ~/master_toolchain/wiringPi/wiringPi/wiringPi.h
```
Now go ahead and do the following changes (line numbers might not be exactly the same):
```diff
211:
-extern int  wiringPiISR         (int pin, int mode, void (*function)(void)) ;
+extern int  wiringPiISR         (int pin, int mode, void (*function)(void *), void *arg) ;
```
Because we need to explicitly set `CC` and `INCLUDE` we must edit the Makefile aswell:
```bash
$ vi ~/master_toolchain/wiringPi/wiringPi/Makefile
```
We must change the `=` to a `?=` so that we can explicitly set it when we run `make`:
```diff
39: -CC      = gcc
40: -INCLUDE = -I.
39: +CC      ?= gcc
49: +INCLUDE ?= -I.
```
We have to do it for gpio/ and devLib/ aswell:
```
$ vi ~/master_toolchain/pi/wiringPi/devLib/Makefile
```
Do the exact same changes as above.

For the gpio utility we also need to modify the source file gpio.c:
```bash
$ vi ~/master_toolchain/pi/wiringPi/gpio/gpio.c
```
Do the following changes:
```diff
497: -static void wfi (void)
497: +static void wfi (void *arg)
521: -  if (wiringPiISR (pin, mode, &wfi) < 0)
521: +  if (wiringPiISR (pin, mode, &wfi, NULL) < 0)
```
Now open up the makefile:
```bash
$ vi ~/master_toolchain/pi/wiringPi/gpio/Makefile
```
Do the following changes:
```diff
35: -CC      = gcc
35: +CC      ?= gcc
36: -INCLUDE = -I$(DESTDIR)$(PREFIX)/include
36: +INCLUDE ?= -I$(DESTDIR)$(PREFIX)/include
39: -LDFLAGS = -L$(DESTDIR)$(PREFIX)/lib
39: +LDFLAGS ?= -L$(DESTDIR)$(PREFIX)/lib
```
Now we want to compile wiringPi modules and put it in a squashfs directory:
```bash
$ export WIRINGPI=$HOME/master_toolchain/pi/wiringPi/squashfs/usr/local
$ mkdir -p $WIRINGPI
$ cd ~/master_toolchain/pi/wiringPi/wiringPi
$ CC=${CCPREFIX}gcc INCLUDE="-I. -I$PIUSR/include" make
$ DESTDIR=$WIRINGPI PREFIX="" LDCONFIG="" make install
```
Copy header files:
```bash
$ mv $WIRINGPI/include $PIUSR/include/wiringPi
```
Now compile devLib/:
```bash
$ cd ~/master_toolchain/pi/wiringPi/devLib
$ CC=${CCPREFIX}gcc INCLUDE="-I. -I$PIUSR/include" make
$ DESTDIR=$WIRINGPI PREFIX="" LDCONFIG="" make install
```
Copy the headers and libs before we compile the gpio utility:
```bash
$ mv $WIRINGPI/include/* $PIUSR/include/wiringPi
$ rm -r $WIRINGPI/include
$ cp $WIRINGPI/lib/* $PIUSR/lib/
```
Finally compile the gpio utility:
```bash
$ mkdir $WIRINGPI/bin
$ cd ~/master_toolchain/pi/wiringPi/gpio
$ CC=${CCPREFIX}gcc INCLUDE="-I. -I$PIUSR/include -I$PIUSR/include/wiringPi" LDFLAGS="-L$PIUSR/lib" make
$ DESTDIR=$WIRINGPI PREFIX="" LDCONFIG="" sudo make install
```

### Packaging wiringPi into a tcz package

Like we did with picam, we also want to package wiringPi into a package that we can use on our RPi:
```bash
$ cd ~/master_toolchain/pi/wiringPi
$ mksquashfs squashfs/ wiringPi.tcz
```
Now let's install it on our RPi:
```bash
$ scp wiringPi.tcz fgmaster:wiringPi.tcz
$ ssh fgmaster
tc@box:~$ mv wiringPi.tcz /mnt/mmcblk0p2/tce/optional/
tc@box:~$ echo wiringPi.tcz >> /mnt/mmcblk0p2/tce/onboot.lst
```

### Building libevent

Let's build libevent that is used by the main software. Clone the repository and then configure it to be built for our RPi's architecture:
```bash
$ cd ~/master_toolchain/pi
$ git clone https://github.com/libevent/libevent
$ cd libevent
$ export LIBEVENT=$HOME/master_toolchain/pi/libevent/squashfs/usr/local
$ mkdir -p $LIBEVENT
$ ./autogen.sh
$ CC=${CCPREFIX}gcc CXX=${CCPREFIX}g++ CFLAGS="-I$PIUSR/include -L$PIUSR/lib" ./configure --host=arm-rpi-linux-gnueabi --prefix=$LIBEVENT
$ make
$ make install
```
Copy the binaries and header files to `$PIUSR` and package libevent into a tcz package:
```bash
$ cd $LIBEVENT
$ mv ./include/* $PIUSR/include
$ cp ./lib/* $PIUSR/lib
$ rm -r bin/ include/
$ cd ~/master_toolchain/pi/libevent
$ mksquashfs squashfs/ libevent.tcz
```
Now let's install it on our RPi:
```bash
$ scp libevent.tcz fgmaster:libevent.tcz
$ ssh fgmaster
tc@box:~$ mv libevent.tcz /mnt/mmcblk0p2/tce/optional/
tc@box:~$ echo libevent.tcz >> /mnt/mmcblk0p2/tce/onboot.lst
```

### Compiling main software for master

If you have not yet cloned this repository, clone it to `~/Fagelmatare`:
```bash
$ cd ~
$ git clone --recursive https://github.com/Linkaan/Fagelmatare
```
Now we compile our modules:
```bash
$ cd ~/Fagelmatare/modules
$ CC=${CCPREFIX}gcc INCLUDE="-I$PIUSR/include" LINKS="-L$PIUSR/lib" make
```
Now we have setup the essentials to test our SW but first let's setup networking.

### Setting up Wi-Fi

In order to get wifi working we need to install some packages and the firmware for our adapter. This is specific to different wifi adapters but <http://wiki.tinycorelinux.net/wiki:list_of_supported_wifi_devices> contains a semi-complete list for the different wireless NIC chips. For the TP-link WN722 we need to install the firmware for the `Atheros AR9271`, `wifi.tcz` and the `wireless-x.x.xx-piCore-vx+.tcz` package. You can search for wireless in the tiny core application browser:
```bash
$ ssh fgmaster
tc@box:~$ tce-load -wi wifi.tcz
tc@box:~$ tce-ab
1. tce-ab - Tiny Core Extension: Application Browser

   S)earch P)rovides K)eywords or Q)uit: s
2. Enter starting chars of desired extension, e.g. abi: wireless
3. tce - Tiny Core Extension browser

   1. firmware-rpi3-wireless.tcz
   2. wireless-4.4.39-piCore_v7+.tcz
   3. wireless_tools-dev.tcz
   4. wireless_tools-doc.tcz
   5. wireless_tools.tcz

   Enter selection ( 1 - 5 ) or (q)uit: 2
4. Version:        4.4.39-piCore_v7+
   Author:         Various
   Original-site:  http://kernel.org
   Copying-policy: GPL
   Size:           2.7M
   Extension_by:   bmarkus
   Tags:           WIRELESS WIFI NETWORK KERNEL MODULES
   Comments:       Binaries only
                   ----
                   Compiled for piCore 8.x
   Change-log:     ----
   Current:        2016/09/08 First version
   
   :q
5. A)bout I)nstall O)nDemand D)epends T)ree F)iles siZ)e L)ist S)earch P)rovides K)eywords or Q)uit: i
   Downloading: wireless-4.4.39-piCore_v7+.tcz
   Connecting to repo.tinycorelinux.net (89.22.99.37:80)
   wireless-4.4.39-piCo 100% |**************************************|  2656k  0:00:00 ETA
   wireless-4.4.39-piCore_v7+.tcz: OK
6. A)bout I)nstall O)nDemand D)epends T)ree F)iles siZ)e L)ist S)earch P)rovides K)eywords or Q)uit: q
```

Now we need to download the firmware for the Atheros AR9271 chip. Again look at the table at <http://wiki.tinycorelinux.net/wiki:list_of_supported_wifi_devices> to see the required driver. It should be included in the `firmware.tcz` package:
```bash
tc@box:~$ tce-load -wi firmware.tcz
```
We want to setup a static ip so we create a `/opt/wlan0.sh` file with the following content:
```bash
#!/bin/sh
wpa_passphrase <replace with SSID> <replace with password> > /etc/wpa_supplicant.conf
wpa_supplicant -i wlan0 -c /etc/wpa_supplicant.conf -B >/dev/null 2>&1
ifconfig wlan0 <replace with wanted IP for RPi> netmask <replace with netmask> up
route add default gw <replace with gateway>
echo nameserver 8.8.8.8 >> /etc/resolv.conf
echo nameserver 8.8.4.4 >> /etc/resolv.conf
```
By default the netmask will be `255.255.255.0` but you need to configure this for your own network. Now make the script executable:
```bash
tc@box:~$ sudo chmod +x /opt/wlan0.sh
```
Make it run at boot by appending the following lines to `/opt/bootlocal.sh`:
```bash
ifconfig wlan0 up 2>/dev/null
iwlist wlan0 scanning >/dev/null
sleep 1
/opt/wlan0.sh 2>&1 >/tmp/wifi0.log
```
We first bring up the wlan0 interface and activate the adapter. We do a dummy scan to make sure the adapter is active and wait 1 second for the scan to finish.

Now plug out the ethernet cable so we can test the wifi connection. To keep the changes after we do a backup and reboot:
```bash
tc@box:~$ filetool.sh -b
tc@box:~$ sudo reboot
```
After the RPi has booted up, try to ping the new IP address and make sure the connection works. If it works then edit the `~/.ssh/config` file to use the new IP for the master RPi by changing `HostName`.

### Sneak-peak of core functionality (optional)

This step is optional but is recommended to make sure everything thus far is setup correctly. We want to test the core module and picam.

Sync the core module to the RPi:
```bash
$ cd ~/Fagelmatare/modules/core
$ rsync -ravh fagelmatare-core fgmaster:fagelmatare-core
```
SSH into the RPi and let's setup a directory used by picam for storage:
```bash
tc@box:~$ sudo mkdir ~/picam
tc@box:~$ (cd /dev/shm && mkdir rec ; mkdir state ; mkdir hooks)
tc@box:~$ ln -nsf ~/picam/archive /dev/shm/rec/archive
tc@box:~$ ln -nsf /dev/shm/rec ~/picam/rec
tc@box:~$ ln -nsf /dev/shm/hooks ~/picam/hooks
tc@box:~$ ln -nsf /dev/shm/state ~/picam/state
```
If you also want to test the livestreaming part you must first [setup the Linux server](#setting-up-linux-server). However that is not necessary to test core functionality so I recommend doing that later.

Now before we run picam we need to know the alsa device name of our microphone as was [previously described how to find](#testing-picam.tcz). Also if you want to test the livestreaming part you need to make sure that the Linux server is accessible by your picam (try pinging the Linux server from the RPi).

Now replace hw:1,0 with your alsa device name (default is hw:0,0 so you can try that if you chose the default setting before) and invoke the command:
```bash
tc@box:~$ (cd ~/picam ; picam --alsadev hw:1,0 --time --timeformat "%a %b %d %H:%M:%S" --hflip --vflip --vfr -v 8400000 > picam.log 2&>1 &)
```
Or to try livestreaming (replace the IP with the actual ip of your Linux server):
```bash
tc@box:~$ (cd ~/picam ; picam --alsadev hw:1,0 --time --timeformat "%a %b %d %H:%M:%S" --hflip --vflip --vfr -v 8400000 --tcpout udp://<replace with IP of server>:8181 > picam.log 2&>1 &)
```
Look at the `~/picam/picam.log` file to see if picam started correctly, otherwise make sure the camera and microphone is connected (look at how to setup picam again).

Now we start our core module:
```bash
tc@box:~$ sudo ~/fagelmatare-core > ~/fagelmatare.log 2&>1 &
```
Put your hand in front of the PIR sensor to trigger a recording or if you are really lazy you can also send a `SIGTSTP` signal to fagelmatare-core to fake it:
```bash
tc@box:~$ sudo kill -TSTP `pidof fagelmatare-core`
```
If you look at `~/picam/picam.log` file again you should see that picam recorded a video. If nothing happen try doing it again and make sure the sensor is properly connected.

Back at our main machine let's try syncing the recording(s):
```bash
$ rsync -ravh fgmaster:picam/archive/* ~/test-videos/
```
Now try playing the video(s) in ~/test-videos/ with VLC or similar. If you have setup the livestreaming part also check so that works.

### Setting up network over usb

This step is only viable if you use the RPi zero. Otherwise you must use a crossover ethernet cable for communication between master and slave which isn't covered in this guide.

In order to communicate with the slave RPi we need to setup a network over usb. This is called tethering, we need to share internet access and setup IP forwarding in the kernel.

First of all we need to install iptables to create a bridge between wlan0 and usb0:
```bash
tc@box:~$ tce-load -wi iptables
```
Next we need to configure iptables using the following script (put in `/home/tc/iptablesconf.sh`):
```bash
#!/bin/sh
IPT=/usr/local/sbin/iptables
LOCAL_IFACE=usb0
INET_IFACE=wlan0
INET_ADDRESS=<replace with IP of master RPi>

# Flush the tables
$IPT -F INPUT
$IPT -F OUTPUT
$IPT -F FORWARD

$IPT -t nat -P PREROUTING ACCEPT
$IPT -t nat -P POSTROUTING ACCEPT
$IPT -t nat -P OUTPUT ACCEPT

# Allow forwarding packets:
$IPT -A FORWARD -p ALL -i $LOCAL_IFACE -j ACCEPT
$IPT -A FORWARD -i $INET_IFACE -m state --state ESTABLISHED,RELATED -j ACCEPT

# Packet masquerading
$IPT -t nat -A POSTROUTING -o $INET_IFACE -j SNAT --to-source $INET_ADDRESS
```
Don't forget to change the `INET_ADDRESS` variable to match the ip of the master RPi.

Next we run the script and save the configuration file to `/etc/iptables.conf` with iptables-save:
```bash
tc@box:~$ chmod +x iptablesconf.sh
tc@box:~$ sudo ./iptablesconf.sh
tc@box:~$ sudo iptables-save | sudo tee /opt/iptables.conf
```
Next we enable ipv4 forwarding in the kernel and also load our iptables configuration and usb0 interface on boot by adding the following to `/opt/bootlocal.sh`:
```bash
/sbin/ifconfig usb0 10.0.1.1 up
echo 1 > /proc/sys/net/ipv4/ip_forward
/usr/local/sbin/iptables-restore < /opt/iptables.conf
```
Now make the changes persistent with `filetool.sh -b` and reboot.

## Configuring our slave

Make sure you have installed and configured tiny core linux on the slave RPi as described in the [Installing tiny core linux](#installing-tiny-core-linux) section.

Again for the following procedures if you are using a RPi zero as the slave you must have a USB ethernet adapter or a TTL Console Debug Cable, however it is much easier to just configure the slave on the master RPi so that you can use ethernet. However, after setting up network over usb you can switch to the RPi zero.

### Setting up network over usb

This step is only viable if you use the RPi zero. Otherwise you must use a crossover ethernet cable to connect to the master which isn't covered in this guide.

Find the IP of your RPi. How to do this is described in the [Preparing the SD-card](preparing-the-sd-card) section.

First of all to make stuff easier let's setup in a similar way as we did with the master a ssh config entry called fgslave so we don't have to retype the ip of our rpi all the time by editing the `~/.ssh/config` file and appending this as the slave entry:
```bash
Host fgslave
  HostName <replace with IP of RPi>
  User tc
```
SSH into the slave RPi to see if it works:
```bash
$ ssh fgslave
tc@box:~$ exit
```

### Adding our ssh key to authorized_keys

Since we already have a working ssh key setup when we configured the master we can just use that one aswell for the slave.

Copy the same public key as you did for master and ssh into slave and paste it into `~/.ssh/authorized_keys`:
```bash
$ ssh fgslave
tc@box:~$ mkdir ~/.ssh
tc@box:~$ touch ~/.ssh/authorized_keys
tc@box:~$ vi ~/.ssh/authorized_keys
```

Paste in your public key with CTRL+SHIFT+V when you are in edit mode (insert mode). Now close the ssh connection and try connecting again to make sure things are working:
```bash
tc@box:~$ exit
$ ssh fgslave
```
Now if you can connect without the password change it to something secure instead of having the default `piCore` password:
```bash
tc@box:~$ passwd
```

Now we can go ahead and configure network over usb. Back on our main computer let's copy all the required usb modules that we need.

Start by downloading the kernel modules and unpack:
```bash
$ mkdir -p ~/slave_toolchain/kernel_modules
$ cd ~/slave_toolchain/kernel_modules
$ wget tinycorelinux.net/8.x/armv6/releases/RPi/src/kernel/4.4.39-piCore+_modules.tar.xz
$ tar xJf 4.4.39-piCore+_modules.tar.xz
```
Now let's copy over the required modules to a directory that we can put into a squash filesystem. We start by creating the necessary directories:
```bash
$ mkdir -p usb_modules/lib/modules/kernel/drivers/usb/gadget/legacy
$ mkdir -p usb_modules/lib/modules/kernel/drivers/usb/gadget/function/
$ mkdir -p usb_modules/lib/modules/kernel/drivers/usb/gadget/udc/
$ mkdir -p usb_modules/lib/modules/kernel/drivers/usb/dwc2
```
Now copy over the actual kernel modules:
```bash
$ cp 4.4.39-piCore+/kernel/drivers/usb/gadget/legacy/g_ether.ko usb_modules/lib/modules/kernel/drivers/usb/gadget/legacy
$ cp 4.4.39-piCore+/kernel/drivers/usb/gadget/libcomposite.ko usb_modules/lib/modules/kernel/drivers/usb/gadget
$ cp 4.4.39-piCore+/kernel/drivers/usb/gadget/function/usb_f_rndis.ko usb_modules/lib/modules/kernel/drivers/usb/gadget/function/
$ cp 4.4.39-piCore+/kernel/drivers/usb/gadget/function/u_ether.ko usb_modules/lib/modules/kernel/drivers/usb/gadget/function/
$ cp 4.4.39-piCore+/kernel/drivers/usb/gadget/udc/udc-core.ko usb_modules/lib/modules/kernel/drivers/usb/gadget/udc/
$ cp 4.4.39-piCore+/kernel/drivers/usb/dwc2/dwc2.ko usb_modules/lib/modules/kernel/drivers/usb/dwc2
```
Now we create the squashfs package:
```bash
$ mksquashfs usb_modules/ usb_modules.tcz
```
Let's install the package by copying it to our slave RPi and adding it to onboot.lst:
```bash
$ scp usb_modules.tcz fgslave:/mnt/mmcblk0p2/tce/optional/usb_modules.tcz
$ ssh fgslave
tc@box:~$ echo usb_modules.tcz >> /mnt/mmcblk0p2/tce/onboot.lst
```
Also make sure the modules.dep is correct by calling `depmod -a` and use filetool.sh to make changes persistent:
```bash
tc@box:~$ sudo depmod -a
tc@box:~$ filetool.sh -b
```
Let's mount the root partition to be able to change config.txt:
```bash
tc@box:~$ sudo mount /dev/mmcblk0p1 /mnt/mmcblk0p1
```
We also want to make sure the `dwc2` overlay is enabled on boot, add the following under the RPI-0 section in `/mnt/mmcblk0p1/config.txt`:
```
dtoverlay=dwc2,rpi-sense
```
Now to load the dwc2 and g_ether module and setup our usb0 interface on boot let's add the following content to `/opt/bootlocal.sh`:
```bash
/sbin/modprobe dwc2
/bin/sleep 5 # workaround for a problem not detecting usb devices
/sbin/modprobe g_ether
/sbin/ifconfig usb0 10.0.1.2 netmask 255.255.255.0 up
/sbin/route add default gw 10.0.1.1
echo nameserver 8.8.8.8 >> /etc/resolv.conf
echo nameserver 8.8.4.4 >> /etc/resolv.conf
```

Before rebooting let's also add the kernel modules for the sense hat.

### Enabling sense hat

The modules for the sensehat framebuffer is not enabled by default. Let's copy the required modules in the same way as we did for usb modules. Create the necessary directories:
```bash
$ mkdir -p sensehat_modules/lib/modules/kernel/drivers/mfd/
$ mkdir -p sensehat_modules/lib/modules/kernel/drivers/input/joystick/
$ mkdir -p sensehat_modules/lib/modules/kernel/drivers/video/fbdev/core
```
Now copy over the actual kernel modules:
```bash
$ cp 4.4.39-piCore+/kernel/drivers/mfd/rpisense-core.ko usb_modules/lib/modules/kernel/drivers/mfd
$ cp 4.4.39-piCore+/kernel/drivers/input/joystick/rpisense-js.ko usb_modules/lib/modules/kernel/drivers/input/joystick
$ cp 4.4.39-piCore+/kernel/drivers/video/fbdev/rpisense-fb.ko usb_modules/lib/modules/kernel/drivers/video/fbdev
$ cp 4.4.39-piCore+/kernel/drivers/video/fbdev/core/fb_sys_fops.ko usb_modules/lib/modules/kernel/drivers/video/fbdev/core
$ cp 4.4.39-piCore+/kernel/drivers/video/fbdev/core/sysfillrect.ko usb_modules/lib/modules/kernel/drivers/video/fbdev/core
$ cp 4.4.39-piCore+/kernel/drivers/video/fbdev/core/syscopyarea.ko usb_modules/lib/modules/kernel/drivers/video/fbdev/core
$ cp 4.4.39-piCore+/kernel/drivers/video/fbdev/core/sysimgblt.ko usb_modules/lib/modules/kernel/drivers/video/fbdev/core
```
Now we create the squashfs package:
```bash
$ mksquashfs sensehat_modules/ sensehat_modules.tcz
```
Let's install the package by copying it to our slave RPi and adding it to onboot.lst:
```bash
$ scp sensehat_modules.tcz fgslave:/mnt/mmcblk0p2/tce/optional/sensehat_modules.tcz
$ ssh fgslave
tc@box:~$ echo sensehat_modules.tcz >> /mnt/mmcblk0p2/tce/onboot.lst
tc@box:~$ filetool.sh -b
```
Now halt the both the master and slave RPi and plug out the power to them and transfer the SD-card to your pi zero. It is time to test if network over usb is setup correctly. Connect a OTG cable (micro usb) between your master and slave. Make sure to connect to the micro usb port labeled as USB on the slave, otherwise the master will only power the slave and be able to talk to it. Plug in the power to the master and if everything is setup correctly the pi zero should display a rainbow
pattern on the RGB led matrix but then after it has booted up it shouldn't display anything anymore. SSH into the master and try to ping the slave:
```bash
$ ssh fgmaster
tc@box:~$ ping 10.0.1.2
```
If you can't ping the slave you should make sure the `g_ether` module loaded correctly. Type `lsmod` to see if it is loaded, if not you must go back to see if you missed anything.

SSH into the slave from the master (with the password you configured before) and test that you have a working internet connection:
```bash
tc@box:~$ ssh tc@10.0.1.2
tc@box:~$ ping google.com
```
If you can't ping google.com you should make sure that the default gateway is setup to go through 10.0.1.1 which is our master (type `route -n`).

A trick we can do to directly access our slave from the main computer instead of having to manually open multiple ssh connections is to use a `ProxyCommand` in our `~/.ssh/config` file. Let's modify the slave entry in `~/.ssh/config`:
```bash
Host fgslave
  HostName 10.0.1.2
  User tc
  ProxyCommand ssh -q fgmaster -W [%h]:22
```
The `ssh -W` command will forward packets via the master to the slave and thus creating a transparent ssh connection to the slave. The master RPi works as a proxy in this case. Try accessing the slave from the main computer and we can also take this opportunity to install rsync + openssl devel package:
```bash
$ ssh fgslave
tc@box:~$ tce-load -wi rsync.tcz
tc@box:~$ tce-load -wi openssl-dev.tcz
tc@box:~$ exit
```
You should not be prompted for a password since the ssh keys are already configured.

Now everything should be setup to configure crosstool-ng.

### Building and configuring crosstool-ng for slave

In order to compile software for our slave RPi we need to configure the cross compiler for our slave aswell in the same manner as we did for our master RPi.

Assuming you have already configured the cross compiler for the master you should already have the required dependencies installed, if not see the section on how to configure the cross compiler for the master.

We create a new directory for our slave toolchain.
```bash
$ mkdir ~/slave_toolchain
$ cd ~/slave_toolchain
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
$ ./configure --prefix=$HOME/slave_toolchain/cross
```
If configure completes without any errors we can compile it, otherwise install any dependencies it says are missing.
```bash
$ make
$ sudo make install
```
Ok, now we need to configure the cross compiler to be able to compile for our RPi architecture.
```bash
$ export PATH=$PATH:$HOME/slave_toolchain/cross/bin
$ cd ~/slave_toolchain
$ mkdir ctng
$ cd ctng
$ ct-ng menuconfig
```

Now see the [Building and configuring crosstool-ng for master](#building-and-configuring-crosstool-ng-for-master) section for the configuration to use. For the raspberry pi zero it will be the generation 1 configuration.

In order to distinguish between the master and slave cross compiler we want to change these settings:

- Paths and misc options
    - Set "Prefix directory" to "${HOME}/slave_toolchain/cross/x-tools/${CT_TARGET}"
- Toolchain options
    - Set "Tuple's vendor string" to "rpislave"

After you have configured crosstool-ng it is time to compile:
```bash
$ sudo chown -R $(whoami) $HOME/slave_toolchain/cross
$ ct-ng build
```

### Testing our cross compiler

In the same way we tested our master cross compiler we want to do a quick sanity check. We export CCPREFIX to be able to compile and add our prefix directory to the path:
```bash
$ export PATH=$PATH:$HOME/slave_toolchain/cross/x-tools/arm-rpislave-linux-gnueabihf/bin
$ export CCPREFIX="$HOME/slave_toolchain/cross/x-tools/arm-rpislave-linux-gnueabihf/bin/arm-rpislave-linux-gnueabihf-"
```
If everything is setup correctly you should be able to get the current version of the ARM compiler:
```bash
$ arm-rpislave-linux-gnueabihf-gcc --version
```
Now we try to build a simple hello world program in C. Open test.c in your favourite editor (vim, nano, emacs) and add the following content:
```c
#include <stdio.h>

int main() {
    printf("Hello, world!\n");
    return 0;
}
```
Now we try building our "test" binary and copying it to the slave RPi.
```bash
$ $(CCPREFIX)gcc -o test test.c
$ scp test fgslave:test
```
SSH into the RPi and run the program:
```bash
$ ssh fgslave
tc@box:~$ sudo chmod +x test
tc@box:~$ ./test
Hello, world!
```
If the program works it will display "Hello, world!" and you're ready to compile the main software. If it doesn't work for you make sure you configured the cross compiler correctly.

### Testing the sensehat

Let's build a simple program that uses the sensehat to retrieve the temperature, humidity and athmospheric air pressure to make sure things are working. The `sensehat_sensor_test.c` is found under the tests/ directory in this repository:
```bash
$ cd tests/
$ $(CCPREFIX)gcc -o sensehat_sensor_test sensehat_sensor_test.c
$ scp sensehat_sensor_test fgslave:sensehat_sensor_test
$ ssh fgslave
tc@box:~$ sudo chmod +x sensehat_sensor_test
tc@box:~$ ./sensehat_sensor_test
```
You should get a report of the temperature, humidity and athmospheric pressure. Sometimes you need to run the program again to get an accurate athmospheric pressure.

Let's also test the led matrix. Back on our main computer we compile the program which is also in the tests/ directory:
```bash
$ $(CCPREFIX)gcc -o sensehat_rgbmatrix_test sensehat_rgbmatrix_test.c
$ scp sensehat_rgbmatrix_test fgslave:sensehat_rgbmatrix_test
$ ssh fgslave
tc@box:~$ sudo chmod +x sensehat_rgbmatrix_test
tc@box:~$ ./sensehat_rgbmatrix_test
```
If the sensehat framebuffer is configured correctly you should see a quicksort of the leds forming a rainbow pattern. The rgb led matrix is not actually used because it might scare the birds away. If everything has worked thus far it is time to compile the software for our slave RPi. If something is not working make sure to go back and see if you missed any steps.

### Copying dependencies

As we did for the master we need to copy all the required dependencies from the RPi so that we can build libevent and the main software for the slave. Let's put this content in the `~/slave_toolchain/pi/usr` directory that we must create along with subdirectories `include` and `lib`:
```bash
$ export PIUSR=$HOME/slave_toolchain/pi/usr
$ mkdir -p $PIUSR
$ cd $PIUSR
$ mkdir include lib
```
Now we can copy all the RPi libraries that are required to build the main software and libevent. We use rsync for this:
```bash
$ rsync -ravh -L fgslave:/usr/local/include/ $PIUSR/include/
$ rsync -ravh -L fgslave:/usr/local/lib/ $PIUSR/lib/
$ rsync -ravh -L fgslave:/usr/lib/ $PIUSR/lib/
```

### Building libevent

Let's build libevent that is used by the main software. Clone the repository and then configure it to be built for our RPi's architecture:
```bash
$ cd ~/slave_toolchain/pi
$ git clone https://github.com/libevent/libevent
$ cd libevent
$ export LIBEVENT=$HOME/slave_toolchain/pi/libevent/squashfs/usr/local
$ mkdir -p $LIBEVENT
$ ./autogen.sh
$ CC=${CCPREFIX}gcc CXX=${CCPREFIX}g++ CFLAGS="-I$PIUSR/include -L$PIUSR/lib" ./configure --host=arm-rpislave-linux-gnueabi --prefix=$LIBEVENT
$ make
$ make install
```
Copy the binaries and header files to `$PIUSR` and package libevent into a tcz package:
```bash
$ cd $LIBEVENT
$ mv ./include/* $PIUSR/include
$ cp ./lib/* $PIUSR/lib
$ rm -r bin/ include/
$ cd ~/slave_toolchain/pi/libevent
$ mksquashfs squashfs/ libevent.tcz
```
Now let's install it on our slave RPi:
```bash
$ scp libevent.tcz fgslave:libevent.tcz
$ ssh fgslave
tc@box:~$ mv libevent.tcz /mnt/mmcblk0p2/tce/optional/
tc@box:~$ echo libevent.tcz >> /mnt/mmcblk0p2/tce/onboot.lst
```

## Compiling the main software for slave

## Setting up Linux server

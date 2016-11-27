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
   $ sudo fdisk -u /dev/mmcblk0
   ```
   Now list partitions with 'p' command and write down the starting and ending  sectors of the second partition.
 2. Delete second partition with `d` then recreate it with `n` command.
   Use the same starting sector as deleted had and provide end sectore or size greater than deleted had having enough free space for Mounted Mode. When finished, exit fdisk with 'w' command. Now the partition size increased but file system size is not yet changed.
 3. Reboot piCore. It is necessary to make Kernel aware of changes.
 4. After reboot expand file system to the new partition boundaries with typing  the following command as root:
    ```bash
    $ sudo resize2fs /dev/mmcblk0p2
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
$ sudo mkdir /boot
$ sudo mount -t vfat /dev/mmcblk0p1 /boot/
$ sudo vi /boot/config.txt
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
$ sudo vi /opt/bootlocal.sh
```
Append the following line to the file
```
chmod 777 /dev/vchiq
```
### Installing dependencies

We need to make a backup of the filesystem to keep our changes after the system shuts down. But first we want to install a few dependencies that we will use in our software later. We want to install **alsa-utils.tcz**, **libasound-dev.tcz**, **fontconfig-dev.tcz**, **harfbuzz-dev.tcz** and **rsync.tcz**.
```bash
$ tce-load -wi alsa-utils.tcz
$ tce-load -wi libasound-dev.tcz
$ tce-load -wi harfbuzz-dev.tcz
$ tce-load -wi fontconfig-dev.tcz
$ tce-load -wi rsync.tcz
```
Now we have installed the dependencies for our software. Let's backup the filesystem and reboot.
```bash
$ sudo filetool.sh -b
$ sudo reboot
```

### Building and configuring crosstool-ng

We want to setup our cross compiler to be able to compile the software for our specific RPi architecture. We need to install a few packages that are required to build crosstool-ng which is the cross compiler that we are going to use. This procedure is done on your main Linux machine.

When we try to configure the cross compiler, it should inform about any missing dependencies that you need to install. On debian based systems we start by installing the following packages:
```bash
$ sudo apt-get install flex bison automake gperf libtool patch texinfo ncurses-dev help2man
```
We create a new directory for our master toolchain.
```bash
$ mkdir ~/master_toolchain
$ cd ~/master_toolchain
```
Download the latest version of crosstool-ng from <http://crosstool-ng.org/download/crosstool-ng/> (version 1.22.0 as of right now)
Next we untar crosstool-ng and configure it.
```bash
$ tar xvf crosstool-ng-*.tar.xz
$ cd crosstool-ng
$ ./configure --prefix=$HOME/master_toolchain/cross
```
If configure completes without any errors we can compile it, otherwise install any dependencies it says are missing.
```bash
$ make
$ sudo make install
```
Ok, now we need to configure the cross compiler to be able to compile for our RPi architecture.
```bash
$ cd ~/master_toolchain
$ mkdir ctng
$ cd ctng
$ ct-ng menuconfig
```
Since the architecture for RPi generation 1 and generation 2 are different we need to configure crosstool-ng for our specific RPi.
#### Configuration for RPi gen 2

- Paths and misc options
    - Check "Try features marked as EXPERIMENTAL"
    - Set "Prefix directory" to "/opt/cross/x-tools/${CT_TARGET}"
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
    - Set "binutils version" to "2.25.1"
- C-library
    - Set "C library" to "glibc"
    - Set "glibc version" to "2.22"
- C compiler
    - Check "Show Linaro versions"
    - Set "gcc version" to "linaro-4.9-2015.06"
    - Set "gcc extra config" to "--with-float=hard"
    - Check "Link libstdc++ statically into the gcc binary"
    - Check "C++" under "Additional supported languages"

#### Configuration for RPi gen 1

- Paths and misc options
    - Check "Try features marked as EXPERIMENTAL"
    - Set "Prefix directory" to "/opt/cross/x-tools/${CT_TARGET}"
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
    - Set "binutils version" to "2.25.1"
- C-library
    - Set "C library" to "glibc"
    - Set "glibc version" to "2.22"
- C compiler
    - Check "Show Linaro versions"
    - Set "gcc version" to "linaro-4.9-2015.06"
    - Set "gcc extra config" to "--with-float=hard"
    - Check "Link libstdc++ statically into the gcc binary"
    - Check "C++" under "Additional supported languages"

Now we save our configration and build crosstool-ng with the settings we chose.
```bash
$ sudo chown -R $(whoami) $HOME/master_toolchain/cross
$ ct-ng build
```

### Testing our cross compiler

Before we start building our main software, we want to do a quick sanity check of our ARM compiler. We change the PATH env variable and export CCPREFIX to be able to compile:
```bash
$ export PATH=$PATH:$HOME/master_toolchain/cross
$ export CCPREFIX="$HOME/master_toolchain/cross/x-tools/arm-rpi-linux-gnueabihf/bin/arm-rpi-linux-gnueabihf-"
```
If everything is setup correctly you should be able to get the current version of the ARM compiler:
```bash
$ arm-unknown-linux-gnueabi-gcc --version
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
$ arm-unknown-linux-gnueabi-gcc -o test test.c
$ rsync -rav test pi@<replace with IP of RPi>:test
```
SSH into the RPi with password "piCore" and run the program:
```bash
$ ssh tc@<replace with IP of RPi>
$ sudo chmod +x test
$ test
Hello, world!
```
If the program works it will display "Hello, world!" and you're ready to compile our main software. If it doesn't work make sure you configured the cross compiler correctly.

### Compiling ffmpeg and picam

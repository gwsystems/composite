# Xilinx Zynq Zc702 Board

This document should spell out the details for how to bootup Composite on that board.

## Prerequisites

### Packages

You need arm-none-eabi-gcc installed, git, qemu-system-arm, etc.
```
sudo apt install gcc-arm-none-eabi qemu git build-essential binutils-dev bison flex minicom libssl-dev qemu
```

## Environment

Environment variables expected/safe to set:

```
REALGCC=arm-none-eabi-gcc		#used by musl-gcc
CROSS_COMPILE=arm-none-eabi-		#used by u-boot and composite build
CC=arm-none-eabi-gcc			#not sure you need this but no harm
ARCH=arm				#used in u-boot
```
Commands to set those:
```
export REALGCC=arm-none-eabi-gcc
export CROSS_COMPILE=arm-none-eabi-
export CC=arm-none-eabi-gcc
export ARCH=arm
```
Or, use the script to set the above environment variables for the current session:
```
. tools/arm_setenv
```
(this is in <composite_clone>/ directory)

Verify, if you'd like:
```
echo $REALGCC
echo $CROSS_COMPILE
echo $CC
echo $ARCH
```

## Repositories

### u-boot and composite for qemu

When following the below instructions for cloning, make sure you have the directories something like this:
```
<workspace>/<composite> #it doesn't matter what you name the composite clone
<workspace>/cos_u-boot-xlnx  # this is more important, as the qemu script in <composite>/tools/ directory expects this..
```
<workspace> can be anything, ex: `/home/<user>`.. Most important thing is that, both composite and uboot be at the same level in the directory hierarchy and follow specific name for uboot directory!! Keep this in mind while cloning things below..
        
### u-boot

**This is not working for now**
Get this:https://github.com/gwsystems/u-boot.git
Branch: cos_armv7a_v2020.07

**use this**
https://github.com/gwsystems/u-boot-xlnx.git
Branch: for_cos
        (unfortunately I have no idea what version they based it on)

#### Steps to build

- Set the environment variables.
- `make zynq_zc702_defconfig` from the cloned uboot-xlnx directory
- `make` after that

### Composite 

Get this: https://github.com/gwsystems/composite.git
Branch: loaderarm

#### Steps to build

- Make sure those environment variables are set
- Go into <composite_clone>/src/ directory, simply because a lot of the config and env is not integrated into the `cos` tool for armv7a.
- `make config-armv7a`
- `make init`
- `make all`
- Go back to <composite_clone> directory
- `./cos compose composition_scripts/kernel_test.toml kernel_test_1`

This generates `cos.img.bin` in `system_binaries/kernel_test_1/` directory, which we will boot from u-boot built in the previous step.

### Zynq mkbootimage

Follow the instructions here to be able to build and boot it. This is required for the baremetal boot, which I have not tested yet!

This: https://github.com/antmicro/zynq-mkbootimage.git
Forking is taking too long.

#### Steps I think are,

- clone and go to the project directory
- You should have access to the zynq board files: fsbl.elf, system.bit, boot.bif
- Copy the u-boot.elf from your earlier step, and these 3 files in to the same directory as zynq-mkbootimage cloned directory.
- `mkbootimage boot.bif boot.bin`

## Qemu execution

1. Make sure the paths in `./tools/arm_runqemu.sh` are correct. Mainly, the uboot.elf is expected to be in `<composite_clone>/../cos_u-boot-xlnx/`. 
1. `./tools/arm_runqemu.sh all system_binaries/kernel_test_1/`
1. Key in this command:
   ```
   load mmc 0:1 00100000 cos.img.bin && go 00100000
   ```
   you may execute those commands in two steps. like
   ```
   load mmc 0:1 00100000 cos.img.bin
   go 00100000
   ```
1. This should ideally boot your system up, if you've a working Composite kernel and user-level, you'll see the output for kernel_test running some benchmarks!

## HW execution

**serial communication**
- I used `minicom` on ubuntu. 
   - To install `sudo apt install minicom`
   - To setup, `sudo minicom -s`
   - The serial cable is connected to USB, unless you have more than one such connection, you will see `/dev/ttyUSB0` as your serial USB device. so
   - In "serial port settings", change serial device to be `/dev/ttyUSB0`, change "Hardware flow control" to `No`, keep all other defaults.
   - (optional) I normally change the "History buffer size" to be maximum, which is `5000`. 
   - If you haven't run it with sudo, you'd not be able to save these settings as "save setup as dfl", so it is just an inconvenience but if you prefer saving and loading a non-default config, you may go ahead and run without sudo and choose "save setup as.." option instead.
   - In my case, the default config after these changes is in `/etc/minicom/minirc.dfl` and as below:
   ```
   # Machine-generated file - use "minicom -s" to change parameters.
   pu port             /dev/ttyUSB0
   pu rtscts           No
   pu histlines        5000
   ```

**This is for TFTP boot**
- I did not build a custom uboot image, instead used what was on the Xilinx board, so skipping mkbootimage steps here.

- For TFTP boot to work, your host and the board must be connected to the same local network. Have the tftp server installed on the host. I followed [this](https://linuxhint.com/install_tftp_server_ubuntu/)

- This creates `/srv/tftp` directory on the host. All you need to do after building `Composite` is to copy the `cos.img.bin` to that directory.

- On the board, stop at u-boot prompt:
  - in my setup, i have both the board and host machine connected to the router which provides dhcp IP addresses. so I'm not setting static ip for the board.
  - so all I change is the `serverip` to match my host machine IP, the board acquires its DHCP IP and connects to the host when you do tftp.
  - after you change the environment you need, do a saveenv, so it saves that for consequent boots!
  - read [this](https://www.denx.de/wiki/view/DULG/UBootCmdGroupEnvironment) for how to print, set, save environment on uboot.
  
- Once that's done and you're are ready to boot Composite, enter the following command:
  ```
  tftp 00100000 cos.img.bin && go 00100000
  ```

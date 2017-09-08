Getting started with a native i386 kernel
=========================================

Composite finally has a (very beta, for research purposes only) native kernel
for i386, eliminating the need for a Linux kernel module and the many hacks
required to make Composite and Linux coexist. The following steps will get you
up and running:

1. `$ cd ; mkdir research ; mkdir transfer ; cd research`

2. `$ git clone git@github.com:gparmer/Composite.git ; mv Composite composite`

3. `$ cd composite/src/ ; make config ; make init` 

   Make sure to follow the instructions of `make config` to verify
   that the configuration information is correct.  Be sure to watch
   for errors during `make init`

4. `$ make i386 ; make cp`

   You'll run this every time you write some code and want to recompile and
   test. Note that if you ommit the `i386` or replace it with `linux`, you will
   build the Linux kernel module instead of the native kernel.

5. `$ cd ~/transfer`

6. `$ ./qemu.sh ppos.sh`

   This will run the component specified in the `ppos.sh` runscript in
   `qemu-system-i386`. Use `qemu-g.sh` instead of `qemu.sh` to have qemu start
   in a state where it is waiting for gdb to attach on port 1234.


Installation and Usage Summary
==============================

Example
-------

Shell commands prefixed by `$` are normal user commands, and those prefixed by `#` are executed by root.

1. `$ cd ; mkdir research ; mkdir transfer ; cd research`

2. `$ git clone git@github.com:gparmer/Composite.git ; mv Composite composite`

3. `$ wget http://www.kernel.org/pub/linux/kernel/v2.6/linux-2.6.36.tar.bz2 ; tar xvfj linux-2.6.36.tar.bz2`

4. `$ cd linux-2.6.36/ ; patch -p1 < ../composite/src/platform/linux/patch/linux-2.6.36-cos.patch`

5. `$ cp ../composite/src/platform/linux/patch/dot_config-linux-2.6.36.config .config ; make menuconfig`

   Here the trick is, of course, to make your kernel work on your hardware.  Have fun!
   
6. `$ make ; make modules`

7. Make sure that you edit `/etc/default/grub` if necessary before continuing.

8. `# make install; make modules_install ; mkinitramfs -o /boot/initrd.img-2.6.36 2.6.36 ; update-grub` 

   Make sure that you edit `/etc/default/grub` before updating grub.

9. `$ cd ../composite/src/ ; make config ; make init` 

   Make sure to follow the instructions of `make config` to verify
   that the configuration information is correct.  Be sure to watch
   for errors during `make init`

10. `$ make ; make cp`

Recompile Steps
---------------

   The following steps will need to be run every time you write some code and want to recompile and test. Notice that they are all superuser. Since you cannot `sudo cd` it is recommended you just do `sudo -i` or `sudo su` and really become superuser at this point.

1. `# cd ; mkdir experiments ; cd experiments ; cp ~yourusername/transfer/* .`

2. `# make init ; sh unit_torrent.sh`

    Only use `make init` once per reboot.

3. `$ dmesg | less`

    See the bottom of the kernel log for output from *Composite*.

4. `# make`

    Use `make` to run the system repeatedly (as opposed to `make init`).

5. `# sh unit_torrent.sh`

Shell Setup
-----------

To get into the write code, compile, run, debug, write code, loop, I
setup my system using `screen` to have 4 consoles:

1. Used to view *Composite* output (i.e. run `dmesg | less` in step 12.).

2. `emacs -nw` to do development.

3. `bash` to run `make ; make cp` (step 9).

4. Root shell to run `make ; sh runscript.sh` (steps 13 and 14).

Development Loop
----------------

1. Modify code.

2. Compile (step 9).

3. Fix compilation errors/warnings and goto 2.

4. Update the kernel and load it (step 13).

5. Run the runscript (step 14).

6. Check output (step 12), and goto 1.


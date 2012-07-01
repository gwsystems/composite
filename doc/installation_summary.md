Installation Summary
====================

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

7. `$ make install; make modules_install ; mkinitramfs -o /boot/initrd.img-2.6.36 2.6.36 ; update-grub` 

   Make sure that you edit `/etc/default/grub` before updating grub.

8. `cd ../composite/src/ ; make config ; make init ; make cp` 

   Make sure to follow the instructions of `make config` to verify
   that the configuration information is correct.  Be sure to watch
   for errors during `make init`

9. ` # cd ; mkdir experiments ; cp ~yourusername/transfer/* .`

10. `# make init ; sh unit_torrent.sh`

    Only use `make init` once per reboot.

11. `$ dmesg | less`

    See the bottom of the kernel log for output from *Composite*.

12. `# make ; sh unit_torrent.sh`

    Use `make` to run the system repeatedly (as opposed to `make init`).

Shell setup
-----------

To get into the write code, compile, run, debug, write code, loop, I
setup my system using `screen` to have 4 consoles:

1. Used to view *Composite* output (i.e. run `dmesg | less`).

2. `emacs -nw` to do development.

3. `bash` to run `make ; make cp`.

4. Root shell to run `make ; sh runscript.sh`.

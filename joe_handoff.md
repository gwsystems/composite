# Composite armv7 port handoff

The first step in this project is two download these two pdf, and try out pdf readers
until you find one which can search 3k page pdfs reasonably quickly.  

These are the only well written or complete specifications related to this project
that I have found.  You could write the entire port with only these
two and a compiler specification if you felt like reimplementing everything from linker
to u-boot to compilier-rt.  

https://www.xilinx.com/support/documentation/user_guides/ug585-Zynq-7000-TRM.pdf
https://static.docs.arm.com/ddi0406/cd/DDI0406C_d_armv7ar_arm.pdf

### source code

These are the places I suggest starting to read for context.  

 - composite/src/platform/cav7
 	contains most of the source code specific to armv7
	
	self contained makefile, not integrated with greater cos build system
	
	start.S contains many of the interesting platform specific primatives and symbols
	afaik this was taken by renyu from another open source project. Before publishing,
	this and a few other parts need attribution.

 - composite/src/kernel/include/*
 	obviously very important to understand.  There are a few changes from mainline
	by me, and a few from renyu.  

 - composite/boot-cos
	There are instructions for building u-boot for this board.  

### tools

 - gcc-arm-none-eabi
	https://gcc.gnu.org/onlinedocs/gcc/ARM-Options.html
 - u-boot
 	it is necessicary to use the xilinx fork of u-boot with support for this board.
	xilinx documentation is split between pdfs that float around online, and
	a atlassian wiki

 - qemu
 	has support for this specific board as well as an idealized armv7 board



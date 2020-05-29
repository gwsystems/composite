#!/bin/sh

cp omp_sparselu_for_bots.o llboot.o
./cos_linker "llboot.o, :" ./gen_client_stub

#cp llboot_comp.o llboot.o
#cp omp_sparselu_for_bots.o boot.o
#cp test_boot.o dummy1.o
#cp test_boot.o dummy2.o
#./cos_linker "llboot.o, ;dummy1.o, ;capmgr.o, ;dummy2.o, ;*boot.o, :boot.o-capmgr.o" ./gen_client_stub

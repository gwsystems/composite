#!/bin/sh
cp llboot_test.o llboot.o
./cos_linker "llboot.o, ;sl_voter_cpt.o, ;test_replica.o, :test_replica.o-sl_voter_cpt.o" ./gen_client_stub

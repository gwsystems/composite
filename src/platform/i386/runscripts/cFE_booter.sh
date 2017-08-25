#!/bin/sh

cp cFE_booter.o llboot.o
./cos_linker 'llboot.o, ;sample_lib.o, ;sample_app.o, ;to_lab.o, ;ci_lab.o, ;sch_lab.o, :sample_app.o-llboot.o;sample_lib.o-llboot.o;to_lab.o-llboot.o;ci_lab.o-llboot.o;sch_lab.o-llboot.o' ./gen_client_stub

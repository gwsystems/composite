cp llboot_test.o llboot.o
./cos_linker 'llboot.o, ;cFE_booter.o, ;sample_lib.o, ;sample_app.o, ;sch_lab.o, :sample_app.o-cFE_booter.o;sample_lib.o-cFE_booter.o;sch_lab.o-cFE_booter.o' ./gen_client_stub

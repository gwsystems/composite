cp llboot_test.o llboot.o
cp cFE_booter.o sl_cFE.o
./cos_linker 'llboot.o, ;sl_cFE.o, ;sample_lib.o, ;sample_app.o, ;sch_lab.o, :sample_app.o-sl_cFE.o;sample_lib.o-sl_cFE.o;sch_lab.o-sl_cFE.o' ./gen_client_stub

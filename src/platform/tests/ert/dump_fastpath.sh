#!/bin/sh

gcc -Wall -Wextra -O3 -I/home/gparmer//research//composite/src//kernel/include/shared/ -I/home/gparmer//research//composite/src//components//include/ -o ert ert.c

objdump -S `objdump -t ert | \
sort | \
awk 'BEGIN {s = e = 0;} { if ($6 == "do_lookups") {s = $1;} else if (s != 0 && e == 0) {e = $1;}} END {print "--start-address=0x"s " --stop-address=0x" e;}'` ert

objdump -S `objdump -t ert | \
sort | \
awk 'BEGIN {s = e = 0;} { if ($6 == "do_captbllkups") {s = $1;} else if (s != 0 && e == 0) {e = $1;}} END {print "--start-address=0x"s " --stop-address=0x" e;}'` ert

echo "\n[Generated lookup code for a 4 level tree enclosed between the nops]"

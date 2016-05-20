#include <cos_component.h>
#include <pong.h>
#include <quarantine.h>

//volatile int f;
//void call(void) { f = *(int*)NULL; return; }
void call(void) { printc("0123456789ABCDEF spdid %ld.\n", cos_spd_id()); return; }

#include <cos_component.h>
#include <print.h>
#include <pong.h>

volatile int c = 0;
void call(void) { if (c++ == 10) assert(0); return; }

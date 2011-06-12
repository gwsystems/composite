#include <cos_component.h>
#include <print.h>
#include <pong.h>

int c = 0;
void call(void) { if (c++ == 500000) assert(0); return; }
//void call(void) { return; }

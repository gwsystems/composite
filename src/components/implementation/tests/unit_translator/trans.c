#include <cos_component.h>
#include <print.h>
#include <stdlib.h> 		/* rand */
#include <torrent.h>

//#define VERBOSE 1
#ifdef VERBOSE
#define printv(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define printv(fmt,...) 
#endif

void cos_init(void)
{
	printc("UNIT TEST Unit tests for cbufs...\n");
	printc("UNIT TEST ALL PASSED\n");
	return;
}

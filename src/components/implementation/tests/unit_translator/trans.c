#include <cos_component.h>
#include <cos_alloc.h>
#include <print.h>
#include <stdlib.h> 		/* rand */
#include <torrent.h>

//#define VERBOSE 1
#ifdef VERBOSE
#define printv(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define printv(fmt,...) 
#endif

#define SIZE 64

void cos_init(void)
{
	int amnt = 0, left = SIZE;
	long evt;
	td_t td;
	char *params = "1";
	char buffer[SIZE];

	printc("UNIT TEST Unit tests for the translator (type into term)...\n");

	evt = evt_split(cos_spd_id(), 0, 0);
	assert(evt > 0);
	td = tsplit(cos_spd_id(), td_root, params, strlen(params), TOR_READ, evt);
	do {
		evt_wait(cos_spd_id(), evt);
		amnt = tread_pack(cos_spd_id(), td, buffer, left);
		left -= amnt;
		buffer[amnt] = '\0';
		printc("%s", buffer);
		assert(left >= 0);
	} while (left);
	
	printc("UNIT TEST ALL PASSED\n");


	return;
}

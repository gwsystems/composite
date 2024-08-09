#include <llprint.h>

int
main(void)
{
	if (NUM_CPU == 4){
			printc("SUCCESS: CONSTANT VALUE(NUM_CPU) has been set to 4 \n");
	}
	else{
			printc("FAIL: CONSTANT VALUE(NUM_CPU) hasn't been changed \n");
	}

	return 0;
}
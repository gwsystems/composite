#include <cos_component.h>
#include <llprint.h>
#include <blockdev.h>

int
main(void)
{
	char buf[8192] = {0, };

	strcpy(buf, "Hello World!!!");
	strcpy(buf + 4096, "World Hello!!!");

	blockdev_bwrite(buf, 0, 2);

	memset(buf, 0, 8192);

	blockdev_bread(buf, 0, 2);

	printc("%s\n", buf);
	printc("%s\n", buf + 4096);

	printc("Test finished\n");

	while(1) {
	}
}

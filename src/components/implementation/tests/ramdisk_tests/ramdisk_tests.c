#include <cos_component.h>
#include <llprint.h>
#include <blockdev.h>

int
main(void)
{
	char buf[8192] = {0, };

	strcpy(buf, "Hello World!!!");
	strcpy(buf + 4096, "World Hello!!!");

	/**
	 * Read and write tests for now are meaningless,
	 * since the current implementation is calling memcpy
	 * and also return 0
	 */

	printc("ramdisk_tests: try to write\n");
	if(!blockdev_bwrite(buf, 0, 2)) {
		printc("ramdisk tests: write: passed\n");
	} else {
		printc("ramdisk tests: write: failed\n");
	}

	memset(buf, 0, 8192);

	printc("ramdisk_tests: try to read\n");
	if(!blockdev_bread(buf, 0, 2)) {
		printc("ramdisk_tests: read: passed\n");
	} else {
		printc("ramdisk_tests: read: failed\n");
	}

	if (!strcmp(buf, "Hello World!!!") & !strcmp(buf + 4096, "World Hello!!!")) {
		printc("ramdisk_tests: check read value: passed\n");
	} else {
		printc("ramdisk_tests: check read value: failed\n");
	}

	printc("ramdisk_tests: finished\n");

	while (1)
		;
}

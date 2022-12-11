#include <cos_component.h>
#include <llprint.h>
#include <filesystem.h>

int
main(void)
{
	int ret;
	char buf[128];

	printc("ext4_tests: opening a test file.\n");

	int fd = filesystem_fopen("/test.txt", "w+");

	if (fd >= 0) {
		printc("ext4_tests: open: passed\n");
	} else {
		printc("ext4_tests: open: failed\n");
		return -1;
	}

	printc("ext4_tests: writing hello world into the file\n");

	ret = filesystem_fwrite(fd, "Hello World!", 64);
	if (ret == 4096) { // Because of the current implementation for interface, fwrite and fread alwasy do a 4096 length op
		printc("ext4_tests: write: passed\n");
	} else {
		printc("ext4_tests: write: failed\n");
	}

	printc("ext4_tests: reading from the file\n");

	if (!filesystem_fseek(fd, 0, 0)) {
		printc("ext4_tests: seek: passed\n");
	} else {
		printc('ext4_tests: seek: failed\n');
	}

	ret = filesystem_fread(fd, buf, 64);
	if(ret == 4096) { // See above
		printc("ext4_tests: read: passed\n");
	} else {
		printc("ext4_tests: read: failed\n");
	}

	if (!strcmp(buf, "Hello World!")) {
		printc("ext4_tests: check read value: passed\n");
	} else {
		printc("ext4_tests: check read value: failed\n");
	}

	printc("ext4_tests: finished\n");

	while (1)
		;
}

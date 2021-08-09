#include <cos_component.h>
#include <llprint.h>
#include <filesystem.h>

int
main(void)
{
	char buf[128];

	printc("ext4_tests: opening a test file.\n");

	int fd = filesystem_fopen("/test.txt", "w+");

	printc("ext4_tests: writing hello world into the file\n");

	filesystem_fwrite(fd, "Hello World!", 64);

	printc("ext4_tests: reading from the file\n");

	filesystem_fseek(fd, 0, 0);
	filesystem_fread(fd, buf, 64);

	printc("ext4_tests: readed %s\n", buf);

	while (1)
		;
}

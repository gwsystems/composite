#include "string.h"
#include "serial.h"
#include "boot_log.h"
#include "printk.h"

char boot_buffer[1024] = { '\0' };

static const char * boot_log_msgs[] = {
    [BOOT_OK]    = "\033[1;32m  OK  ",
	[BOOT_WARN]  = "\033[1;33m WARN ",
	[BOOT_ERROR] = "\033[1;31mERROR!",
};

static const char *last_message = NULL;

void 
boot_log(const char *s)
{
    last_message = s;

    sprintf(boot_buffer, "\033[0m%s\033[1000C\033[8D[ \033[1;34m....\033[0m ]", s);
    serial__puts(boot_buffer);
}

void
boot_log_finish(enum boot_log_level l)
{
    if (last_message == NULL)
        return;
    
    sprintf(boot_buffer, "\033[1000D\033[0m%s\033[1000C\033[8D[%s\033[0m]\n", last_message, boot_log_msgs[l]);
    serial__puts(boot_buffer);
}


#ifndef _BOOT_LOG_H_
#define _BOOT_LOG_H_

enum boot_log_level {
    BOOT_OK,
    BOOT_WARN,
    BOOT_ERROR,
};


void boot_log(const char *s);
void boot_log_finish(enum boot_log_level l);


#endif

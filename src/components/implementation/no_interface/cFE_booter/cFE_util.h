#ifndef _cFE_util_
#define _cFE_util_

#include <stdio.h>
#include <string.h>

#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cobj_format.h>

#include <llprint.h>


#include "gen/common_types.h"

// These variables store the global SPACECRAFT_ID and CPU_ID
uint32  CFE_PSP_SpacecraftId;
uint32  CFE_PSP_CpuId;

void llprint(const char *s, int len);

int __attribute__((format(printf,1,2))) printc(char *fmt, ...);

#define PANIC(a) panic_impl(__func__, a)

void panic_impl(const char* function, char* message);

void print_with_error_name(char* message, int32 error);

int __isoc99_sscanf(const char *str, const char *format, ...);

#endif

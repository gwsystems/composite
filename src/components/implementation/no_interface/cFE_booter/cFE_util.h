#ifndef _CFE_UTIL_
#define _CFE_UTIL_

#include <stdio.h>
#include <string.h>

#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_debug.h>

#include "gen/common_types.h"

// These variables store the global SPACECRAFT_ID and CPU_ID
uint32 CFE_PSP_SpacecraftId;
uint32 CFE_PSP_CpuId;

#define PANIC(a) panic_impl(__func__, a)

void panic_impl(const char *function, char *message);

void print_with_error_name(char *message, int32 error);

#endif

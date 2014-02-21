#include "include/fpu.h"

PERCPU_VAR(fpu_disabled);
PERCPU_VAR(fpu_last_used);
PERCPU_VAR(fpu_support_fxsr);

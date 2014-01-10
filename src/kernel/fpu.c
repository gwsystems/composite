#include "include/fpu.h"

int fpu_disabled;
struct thread *fpu_last_used;
int fpu_support_fxsr;

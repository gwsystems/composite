#ifndef C_STUB_H
#define C_STUB_H

#if defined(__x86__)
#include "arch/x86/c_stub.h"
#elif defined(__arm__)
#include "arch/arm/c_stub.h"
#else
#error "Undefined architecture!"
#endif

#endif /* C_STUB_H */

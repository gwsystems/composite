#ifndef COS_SERIAL_H
#define COS_SERIAL_H

#if defined(__arm__)
#include "arch/arm/cos_serial.h"
#elif defined(__x86__)
#include "arch/x86/cos_serial.h"
#else
#error "Undefined architecture!"
#endif


#endif /* COS_SERIAL_H */

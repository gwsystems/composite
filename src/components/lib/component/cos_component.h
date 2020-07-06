#ifndef COS_COMPONENT_H
#define COS_COMPONENT_H

#if defined(__arm__)
#include "arch/arm/cos_component.h"
#elif defined(__x86__)
#include "arch/x86/cos_component.h"
#else
#error "Undefined architecture!"
#endif

#endif /* COS_COMPONENT_H */

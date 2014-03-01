#ifndef _PORTS_H_
#define _PORTS_H_

#include "shared/cos_types.h"

__inline__ void outb(u16_t port, u8_t value);
__inline__ u8_t inb(u16_t port);
__inline__ u16_t inw(u16_t port);
__inline__ void io_wait(void);

#endif

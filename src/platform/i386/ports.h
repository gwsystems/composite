#ifndef _PORTS_H_
#define _PORTS_H_

#include "types.h"

__inline__ void outb(uint16_t port, uint8_t value);
__inline__ uint8_t inb(uint16_t port);
__inline__ uint16_t inw(uint16_t port);
__inline__ void io_wait(void);

#endif

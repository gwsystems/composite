#ifndef IO_H
#define IO_H

#include "shared/cos_types.h"

/**
 * Write byte to specific port
 */
static inline void 
outb(u16_t port, u8_t value)
{
    __asm__ __volatile__("outb %1, %0" : : "dN" (port), "a" (value));
}

/**
 * Read byte from port
 */
static inline u8_t 
inb(u16_t port)
{
    u8_t ret;

    __asm__ __volatile__("inb %1, %0"
        : "=a" (ret)
        : "dN" (port));
    
    return ret;
}

/**
 * Read word (16 bit value) from port
 */
static inline u16_t
inw(u16_t port)
{
    u16_t ret;
    __asm__ __volatile__("inw %1, %0"
        : "=a" (ret)
        : "dN" (port));
    
    return ret;
}

static inline void
io_wait(void)
{
    /* Port 0x80 is being used for 'checkpoints' during POST,
     * and Linux thinks at this point its free for us to use as a 
     * "wait" op 
     */
    __asm__ __volatile__("outb %%al, $0x80"
            : : "a"(0) );
}


#endif /* IO_H */

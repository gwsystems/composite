#include "ports.h"
#include "types.h"

/**
 * Write byte to specific port
 */
__inline__ void 
outb(uint16_t port, uint8_t value)
{
    __asm__ __volatile__("outb %1, %0" : : "dN" (port), "a" (value));
}

/**
 * Read byte from port
 */
__inline__ uint8_t 
inb(uint16_t port)
{
    uint8_t ret;

    __asm__ __volatile__("inb %1, %0"
        : "=a" (ret)
        : "dN" (port));
    
    return ret;
}

/**
 * Read word (16 bit value) from port
 */
__inline__ uint16_t
inw(uint16_t port)
{
    uint16_t ret;
    __asm__ __volatile__("inw %1, %0"
        : "=a" (ret)
        : "dN" (port));
    
    return ret;
}

__inline__ void
io_wait(void)
{
    /* Port 0x80 is being used for 'checkpoints' during POST,
     * and Linux thinks at this point its free for us to use as a 
     * "wait" op 
     */
    __asm__ __volatile__("outb %%al, $0x80"
            : : "a"(0) );
}


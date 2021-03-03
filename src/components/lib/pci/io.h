#ifndef IO_H
#define IO_H

#include <cos_types.h>

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

static inline void
outl(u16_t port, u32_t value)
{
    __asm__ __volatile__("outl %1, %0"
        : 
        : "dN" (port), "a" (value));
}

static inline u32_t
inl(u16_t port)
{
    u32_t ret;

    __asm__ __volatile__("inl %1, %0"
        : "=a" (ret)
        : "dN" (port));

    return ret;
}
#endif /* IO_H */
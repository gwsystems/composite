#define prevent_tail_call(ret) __asm__ ("" : "=r" (ret) : "m" (ret))

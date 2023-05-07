#pragma once

#ifndef NULL
#define NULL ((void *)0)
#endif
#ifndef unlikely
#define unlikely(p) __builtin_expect((p), 0)
#endif
#ifndef likely
#define likely(p) __builtin_expect((p), 1)
#endif

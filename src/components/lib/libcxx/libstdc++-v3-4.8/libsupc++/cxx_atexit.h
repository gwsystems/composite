#ifndef CXX_ATEXIT_H__
#define CXX_ATEXIT_H__

extern "C" int __cxa_atexit(void (*f)(void*), void *arg, void *dso_handle);

#endif

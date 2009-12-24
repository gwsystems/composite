#ifndef _STRING_H
#define _STRING_H

#include <sys/cdefs.h>
#include <sys/types.h>

__BEGIN_DECLS

char *strcpy(char *dest, const char *src) __THROW;

void *memccpy(void *dest, const void *src, int c, size_t n) __THROW;
void *memmove(void *dest, const void *src, size_t n) __THROW;

int memccmp(const void *s1, const void *s2, int c, size_t n) __THROW __pure;

void* memset(void *s, int c, size_t n) __THROW;
int memcmp(const void *s1, const void *s2, size_t n) __THROW __pure;
void* memcpy(void *dest, const void *src, size_t n) __THROW;

char *strncpy(char *dest, const char *src, size_t n) __THROW;
int strncmp(const char *s1, const char *s2, size_t n) __THROW __pure;
char *strncat(char *dest, const char *src, size_t n) __THROW;

int strcmp(const char *s1, const char *s2) __THROW __pure;

size_t strlen(const char *s) __THROW __pure;

char *strstr(const char *haystack, const char *needle) __THROW __pure;

char *strdup(const char *s) __THROW __attribute_malloc__ ;

char *strchr(const char *s, int c) __THROW __pure;
char *strrchr(const char *s, int c) __THROW __pure;

char *strcat(char *dest, const char *src) __THROW;

size_t strspn(const char *s, const char *_accept) __THROW;
size_t strcspn(const char *s, const char *reject) __THROW;

char *strpbrk(const char *s, const char *_accept) __THROW;
char *strsep(char **stringp, const char *delim) __THROW;

void* memchr(const void *s, int c, size_t n) __THROW __pure;
#ifdef _GNU_SOURCE
void* memrchr(const void *s, int c, size_t n) __THROW __pure;
#endif

/* I would like to make this const, but Paul Jarc points out it has to
 * be char* :-( */
char *strerror(int errnum) __THROW;
/* work around b0rken GNU crapware like tar 1.13.19 */
#define strerror strerror
int strerror_r(int errnum,char* buf,size_t n) __THROW __attribute_dontuse__;

#ifdef _GNU_SOURCE
const char *strsignal(int signum) __THROW;
void *memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen) __THROW;

char *strndup(const char *s,size_t n) __THROW __attribute_malloc__ ;
#endif

char *strtok(char *s, const char *delim) __THROW;
char *strtok_r(char *s, const char *delim, char **ptrptr) __THROW;

size_t strlcpy(char *dst, const char *src, size_t size) __THROW;
size_t strlcat(char *dst, const char *src, size_t size) __THROW;

int strcoll(const char *s1, const char *s2) __THROW;
size_t strxfrm(char *dest, const char *src, size_t n) __THROW;

#ifdef _BSD_SOURCE
#include <strings.h>
#endif

char *stpcpy(char *dest, const char *src);

#ifdef _GNU_SOURCE
int ffsl(long i) __THROW;
int ffsll(long long i) __THROW;
#endif

__END_DECLS

#endif

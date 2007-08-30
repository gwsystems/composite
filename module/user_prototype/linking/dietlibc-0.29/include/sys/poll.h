#ifndef _SYS_POLL_H
#define _SYS_POLL_H

#include <sys/cdefs.h>

__BEGIN_DECLS

enum {
  POLLIN	= 0x0001,
#define POLLIN		POLLIN
  POLLPRI	= 0x0002,
#define POLLPRI		POLLPRI
  POLLOUT	= 0x0004,
#define POLLOUT		POLLOUT
  POLLERR	= 0x0008,
#define POLLERR		POLLERR
  POLLHUP	= 0x0010,
#define POLLHUP		POLLHUP
  POLLNVAL	= 0x0020,
#define POLLNVAL	POLLNVAL
  POLLRDNORM	= 0x0040,
#define POLLRDNORM	POLLRDNORM
  POLLRDBAND	= 0x0080,
#define POLLRDBAND	POLLRDBAND
  POLLWRBAND	= 0x0200,
#define POLLWRBAND	POLLWRBAND
  POLLMSG	= 0x0400,
#define POLLMSG		POLLMSG
/* POLLREMOVE is for /dev/epoll (/dev/misc/eventpoll),
 * a new event notification mechanism for 2.6 */
  POLLREMOVE	= 0x1000,
#define POLLREMOVE	POLLREMOVE
};

#if defined(__sparc__) || defined (__mips__)
#define POLLWRNORM	POLLOUT
#else
#define POLLWRNORM	0x0100
#endif

struct pollfd {
  int fd;
  short events;
  short revents;
};

extern int poll(struct pollfd *ufds, unsigned int nfds, int timeout) __THROW;

__END_DECLS

#endif	/* _SYS_POLL_H */

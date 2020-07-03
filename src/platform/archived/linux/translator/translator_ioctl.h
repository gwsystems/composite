#ifndef TRANSLATOR_IOCTL_H
#define TRANSLATOR_IOCTL_H

#define TRANS_SET_CHANNEL    _IOR(0, 1, int)
#define TRANS_GET_CHANNEL    _IOW(0, 2, int)
#define TRANS_SET_DIRECTION  _IOR(0, 3, int)

#ifndef __KERNEL__
#include <stdio.h>
#include <sys/ioctl.h>
#include <errno.h>

static void
trans_ioctl_set_channel(int fd, int chan)
{
	int ret;

	if ((ret = ioctl(fd, TRANS_SET_CHANNEL, chan))) {
		perror("Could not set channel");
		printf("ioctl returned %d\n", ret);
		exit(-1);
	}
	return;
}

static int
trans_ioctl_get_channel(int fd)
{
	int ret, chan;

	if ((ret = ioctl(fd, TRANS_GET_CHANNEL, &chan))) {
		perror("Could not get channel");
		printf("ioctl returned %d\n", ret);
		exit(-1);
	}
	
	return chan;
}

static void
trans_ioctl_set_direction(int fd, int direction)
{
	int ret;

	if ((ret = ioctl(fd, TRANS_SET_DIRECTION, direction))) {
		perror("Could not set channel direction");
		printf("ioctl returned %d\n", ret);
		exit(-1);
	}
	return;
}

#endif
#endif

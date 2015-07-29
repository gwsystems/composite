#ifndef RUMPCALLS_H
#define RUMPCALLS_H

struct cos_rumpcalls
{
	int (*cos_print)(char s[], int ret);
};

#endif /* RUMPCALLS_H */

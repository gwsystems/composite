#ifndef SRV_DUMMY_TYPES_H
#define SRV_DUMMY_TYPES_H

#include <cos_types.h>

typedef enum {
	SRV_DUMMY_HELLO = 0,
	SRV_DUMMY_GOODBYE = 1,
} srv_dummy_api_t;

#define SRV_DUMMY_MAX 2

/* TODO: keys through runscripts!! */
#define SRV_DUMMY_KEY 'A'
#define SRV_DUMMY_INSTANCE(i) (SRV_DUMMY_KEY | (i << 10))

#define SRV_DUMMY_SKEY(i, t) (SRV_DUMMY_INSTANCE(i) + t)
#define SRV_DUMMY_RKEY(i, t) ((1 << 9) | (SRV_DUMMY_INSTANCE(i) + t))

#define SRV_DUMMY_ISTATIC 0
#define SRV_DUMMY_IMAX    1

#endif /* SRV_DUMMY_TYPES_H */

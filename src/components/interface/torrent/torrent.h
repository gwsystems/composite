#ifndef TORRENT_H
#define TORRENT_H

#include <cbuf.h>

/* torrent descriptor */
typedef int tid_t;
const tid_t td_init = 0, td_sink = 0;
typedef enum {
	TOR_WRITE = 0x1,
	TOR_READ  = 0x2,
	TOR_SPLIT = 0x4,
	TOR_ALL   = TOR_WRITE | TOR_READ | TOR_SPLIT /* 0 is a synonym */
} tor_flags_t;

//tid_t tsplit(tid_t td, char *param, int len, tor_flags_t tflags, evtid_t evtid);
struct tsplit_data {
	short int end;
	tor_flags_t tflags;
	long evtid;
	char data[0];
};
tid_t tsplit(tid_t td, int cbid, int sz);

//int tmerge(tid_t td, tid_t td_into, char *param, int len);
int tmerge(tid_t td, tid_t td_into, int cbid, int sz);

//int tread(tid_t td, char *data, int amnt);
int tread(tid_t td, int cbid, int sz);

//int twrite(tid_t td, char *data, int amnt);
int twrite(tid_t td, int cbid, int sz);

//int trmeta(tid_t td, char *key, int flen, char *value, int vlen);
struct trmeta_data {
	short int value, end; /* offsets into data */
	char data[0];
};
int trmeta(tid_t td, int cbid, int sz);

//int twmeta(tid_t td, char *key, int flen, char *value, int vlen);
struct twmeta_data {
	short int value, end; /* offsets into data */
	char data[0];
};
int twmeta(tid_t td, int cbid, int sz);

#endif /* TORRENT_H */ 

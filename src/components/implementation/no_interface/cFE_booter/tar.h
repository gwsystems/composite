#define TAR_BLOCKSIZE 512
#define INT32_MAX 0x7FFFFFF /* 2^31 - 1 */

/*from GNU */
struct posix_header {   /* byte offset */
	char name[100];     /*   0 */
	char mode[8];       /* 100 */
	char uid[8];        /* 108 */
	char gid[8];        /* 116 */
	char size[12];      /* 124 */
	char mtime[12];     /* 136 */
	char chksum[8];     /* 148 */
	char typeflag;      /* 156 */
	char linkname[100]; /* 157 */
	char magic[6];      /* 257 */
	char version[2];    /* 263 */
	char uname[32];     /* 265 */
	char gname[32];     /* 297 */
	char devmajor[8];   /* 329 */
	char devminor[8];   /* 337 */
	char prefix[155];   /* 345 */
	                    /* 500 */
};

uint32 tar_load();

uint32 tar_parse();

uint32 tar_hdr_read(uint32 tar_offset, struct fsobj *file);

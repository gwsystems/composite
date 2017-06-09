#include "cFE_util.h"
#include <string.h>
#include "gen/common_types.h"
#include "gen/osapi.h"
#include "gen/osapi-os-filesys.h"

// Not to be confused with similar OSAL constants.
// These are only to define size of statically allocated data
#define MAX_NUM_FS 3
#define MAX_NUM_FILES 100

#define TAR_BLOCKSIZE 512
#define INT32_MAX 0x7FFFFFF //2^31 - 1

//should be overwritten by linking step in build process
__attribute__((weak)) char _binary_cFEfs_tar_size=0;
__attribute__((weak)) char _binary_cFEfs_tar_start=0;
__attribute__((weak)) char _binary_cFEfs_tar_end=0;

//locations and size of tar
char      *start;
char      *end;
size_t    size;

typedef enum {
    FSOBJ_FILE,
    FSOBJ_DIR,
} fsobj_type_t;

struct fsobj {
    char *name;
    int32 filedes; // 0 for free file, posative for tracked file
    fsobj_type_t type;
    size_t size;
    uint32 refcnt; //number of programs which have opened it
    char *data;
    struct fsobj *next, *prev;
    struct fsobj *child, *parent;   /* child != NULL iff type = dir */
};

struct FS {
    char *devname;
    char *volname;
    char *mountpoint;
    uint32 blocksize;
    uint32 numblocks;
    struct fsobj *root;
    struct FS *next, *prev;
};

/******************************************************************************
** Tar Level Methods
******************************************************************************/

uint32 tar_load();

uint32 tar_parse();

int32 tar_read(uint32 offset, char *buf, uint32 req_sz);

uint32 tar_cphdr(uint32 tar_offset, struct fsobj *file);

uint32 round_to_blocksize(uint32 offset);

static inline uint32 oct_to_dec(char *oct)
{
    int32 i, base;
    int32 tot;

    i = strlen(oct) - 1;

    for (base = 1, tot = 0 ; i >= 0 ; i--, base *= 8) {
        char val = oct[i];

        assert(val <= '7' && val >= '0');
        val = val - '0';
        tot = tot + (val * base);
    }

    return tot;
}

/******************************************************************************
** fsobj Level Methods
******************************************************************************/

uint32 fsobj_init(struct fsobj *o);

uint32 newfile_get(struct fsobj **o);

uint32 file_insert(struct fsobj *o, struct FS *fs);

uint32 insert(struct fsobj *o, struct fsobj *root);

int32 open(const char *path);

int32 close(int32 filedes);

int32 read(int32  filedes, void *buffer, uint32 nbytes);

struct fsobj *find(const char *path, struct fsobj *root);

/******************************************************************************
** fs Level Methods
******************************************************************************/

uint32 mount(const char *devname, char *mountpoint);

uint32 unmount(const char *mountpoint);

uint32 fs_init(struct FS *filesys, char *devname, char *volname, uint32 blocksize, uint32 numblocks);

uint32 newfs_init(char *defilvname, char *volname, uint32 blocksize, uint32 numblocks);

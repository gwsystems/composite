#include "cFE_util.h"

#include "gen/common_types.h"
#include "gen/osapi.h"
#include "gen/osapi-os-filesys.h"

#include <string.h>

#include <cos_component.h>
#include <cos_defkernel_api.h>
#include <cos_kernel_api.h>

// Not to be confused with similar OSAL constants.
// These are only to define size of statically allocated data
#define MAX_NUM_FS 3
#define MAX_NUM_FILES 100

#define TAR_BLOCKSIZE 512
#define INT32_MAX 0x7FFFFFF //2^31 - 1

// a page is 4096, size of f_part is 5 values * 4 bytes
#define F_PART_DATA_SIZE 4096-4*5

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

struct file_position {
    struct f_part *open_part;  //part of file being read/written to
    uint32 part_offset;         //offset into part's data
    uint32 file_offset;         //position within file as a whole
    char *addr;
};

struct fsobj {
    char *name;
    int32 filedes; // 0 for free file, positive for tracked file
    fsobj_type_t type;
    size_t size;
    struct file_position position;
    uint32 refcnt; //number of threads which have opened it
    uint32 mode;
    struct f_part *file_part;
    struct fsobj *next, *prev;
    struct fsobj *child, *parent; // child != NULL iff type = dir
};

typedef enum {
    STATIC,
    DYNAMIC,
} fpart_type;

struct f_part {
    struct fsobj *file;
    struct f_part *next, *prev;
    fpart_type memtype;
    char *data;
};

struct fs {
    char *devname;
    char *volname;
    char *mountpoint;
    uint32 blocksize;
    uint32 numblocks;
    struct fsobj *root;
};

/******************************************************************************
** Tar Level Methods
******************************************************************************/

uint32 FS_entrypoint();
uint32 FS_posttest();
uint32 BFT(struct fsobj *o);

uint32 tar_load();


uint32 tar_parse();

int32 tar_read(uint32 offset, char *buf, uint32 req_sz);

uint32 tar_cphdr(uint32 tar_offset, struct fsobj *file);

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

//this could be more performant bithack
static inline uint32 round_to_blocksize(uint32 offset)
{
    if (offset % TAR_BLOCKSIZE) return offset + (TAR_BLOCKSIZE - (offset % TAR_BLOCKSIZE));
    return offset;
}


static inline char *path_to_name(char *path){

    uint32 path_len = strlen(path), offset;
    assert(path[path_len - 2] != '/' && path_len > 1);

    // offset is second to last char in path, because last char may be trailing /
    for ( offset = path_len - 2 ; path[offset] != '/' && offset > 0 ; offset--) {
        //do nothing
    }
    assert(0 < strlen(path + offset + 1));
    return path + offset + 1;
}

/******************************************************************************
** fsobj Level Methods
******************************************************************************/
static inline int32 rm(struct fsobj *o)
{
    assert(o && o->child == NULL);

    // if o is first in list of children
    if (o->prev == NULL && o->parent) {
        assert(o->parent->child == o);
        // if next = null this still works
        o->parent->child = o->next;
    }

    // these still work if next or prev = null
    if (o->prev) o->prev->next = o->next;
    if (o->next) o->next->prev = o->prev;

    //we do not do deallocate file data but we do reuse fsobj
    o->name = NULL;
    o->filedes = 0;
    o->size = 0;
    o->refcnt = 0;
    o->mode = 0;
    o->file_part = NULL;
    return OS_FS_SUCCESS;
}

uint32 newpart_get(struct f_part **part);

uint32 newfile_get(struct fsobj **o);

uint32 file_insert(struct fsobj *o, char *path);

int32 file_open(const char *path);

int32 file_close(int32 filedes);

void file_rewinddir();

int32 file_read(int32 filedes, void *buffer, uint32 nbytes);

int32 file_write(int32 filedes, void *buffer, uint32 nbytes);

struct fsobj *file_find(const char *path);

int32 file_mkdir(const char *path, uint32 access);

int32 file_rmdir(const char *path);

int32 file_creat(const char *path);

int32 file_remove(const char *path);

int32 file_rename(const char *old_filename, const char *new_filename);

int32 file_cp(const char *src, const char *dest);

int32 file_mv(const char *src, const char *dest);

int32 file_FDGetInfo(int32 filedes, OS_FDTableEntry *fd_prop);

int32 file_stat(const char *path, os_fstat_t  *filestats);

int32 file_lseek(int32  filedes, int32 offset, uint32 whence);

struct dirent *file_readdir(int32 filedes);
/******************************************************************************
** fs Level Methods
******************************************************************************/

// chk_path checks if a path is the right format for a path, and if it exits
static inline int32 chk_path(const char *path)
{
    if (path == NULL) return OS_FS_ERR_INVALID_POINTER;
    if (strlen(path) > OS_MAX_PATH_LEN) return OS_FS_ERR_PATH_TOO_LONG;
    if (path[0] != '/') return OS_FS_ERR_PATH_INVALID;
    if (file_find(path) == NULL) return OS_FS_ERR_PATH_INVALID;
    return 0;
}

// chk_path_new is for files that should not already be in the filesystem
static inline int32 chk_path_new(const char *path)
{
    if (path == NULL) return OS_FS_ERR_INVALID_POINTER;
    if (strlen(path) > OS_MAX_PATH_LEN) return OS_FS_ERR_PATH_TOO_LONG;
    return OS_FS_SUCCESS;
}

uint32 fs_mount(const char *devname, char *mountpoint);

uint32 fs_unmount(const char *mountpoint);

uint32 fs_init(struct fs *filesys, char *devname, char *volname, uint32 blocksize, uint32 numblocks);

uint32 newfs_init(char *defilvname, char *volname, uint32 blocksize, uint32 numblocks);

int32 rmfs(char *devname);

int32 Filesys_GetPhysDriveName(char * PhysDriveName, char * MountPoint);

int32 Filesys_GetFsInfo(os_fsinfo_t *finesys_info);

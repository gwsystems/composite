#include <string.h>
#include <dirent.h>

#include "gen/common_types.h"
#include "gen/osapi.h"
#include "gen/osapi-os-filesys.h"

#include <cos_component.h>
#include <cos_defkernel_api.h>
#include <cos_kernel_api.h>

#include "cFE_util.h"

// Not to be confused with similar OSAL constants.
// These are only to define size of statically allocated data
#define MAX_NUM_FS 3
#define MAX_NUM_FILES 100
#define MAX_NUM_DIRENT 10

#define TAR_BLOCKSIZE 512
#define INT32_MAX 0x7FFFFFF //2^31 - 1

// a page is 4096, size of f_part is 5 values * 4 bytes
#define F_PART_DATA_SIZE (4096-4*5)

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

/*
 * The structure of the filesystem is stored in the fsobj struct
 * each non-leaf dir points to its first child, which then has a pointer
 * to the next, etc. Each file has a linked list of f_parts to store blocks of data
 */

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

static inline char *path_to_name(char *path)
{
    assert(path);
    uint32 path_len = strlen(path), offset;
    assert(path_len > 1);

    //remove one or more '/' at the end of path
    while(path[strlen(path) - 1] == '/'){
        path[strlen(path) - 1] = 0;
    }

    // iterate from right to left through the path until you find a '/'
    // everything you have iterated through is the name of the file
    for ( offset = path_len - 2 ; path[offset] != '/' && offset > 0 ; offset--) {
        //do nothing
    }
    char *name = path + offset +1;

    assert(0 < strlen(name));
    return name;
}

/******************************************************************************
** fsobj Level Methods
******************************************************************************/
static inline int32 file_rm(struct fsobj *o)
{
    assert(o && o->child == NULL);

    // if o is first in list of children, update parent link to it
    if (o->prev == NULL && o->parent) {
        assert(o->parent->child == o);
        // if next = null this still works
        o->parent->child = o->next;
    }

    // update link from prev still work if next or prev = null
    if (o->prev) {
        assert(o->prev->next == o);
        o->prev->next = o->next;
    }
    // update link from next
    if (o->next) {
        assert(o->next->prev == o);
        o->next->prev = o->prev;
    }
    // there should now be no links within the fs to o

    // we do not do deallocate file data but we do reuse fsobj
    *o = (struct fsobj) {
        .name = NULL
    };

    return OS_FS_SUCCESS;
}

uint32 newpart_get(struct f_part **part);

uint32 newfile_get(struct fsobj **o);

uint32 file_insert(struct fsobj *o, char *path);

int32 file_open(char *path);

int32 file_close(int32 filedes);

int32 file_read(int32 filedes, void *buffer, uint32 nbytes);

int32 file_write(int32 filedes, void *buffer, uint32 nbytes);

struct fsobj *file_find(char *path);

int32 file_create(char *path);

int32 file_remove(char *path);

int32 file_rename(char *old_filename, char *new_filename);

int32 file_cp(char *src, char *dest);

int32 file_mv(char *src, char *dest);

int32 chk_fd(int32 filedes);

int32 file_FDGetInfo(int32 filedes, OS_FDTableEntry *fd_prop);

int32 file_stat(char *path, os_fstat_t  *filestats);

int32 file_lseek(int32  filedes, int32 offset, uint32 whence);

/******************************************************************************
** dirent Level Methods
******************************************************************************/
uint32 newdirent_get(os_dirent_t **dir);

os_dirent_t *dir_open(char *path);

uint32 dir_close(os_dirent_t *dir);

void dir_rewind(os_dirent_t *dir);

os_dirent_t *dir_read(os_dirent_t *dir);

int32 dir_mkdir(char *path);

int32 dir_rmdir(char *path);

/******************************************************************************
** fs Level Methods
******************************************************************************/

// chk_path checks if a path is the right format for a path, and if it exits
static inline int32 chk_path(const char *path)
{
    if (path == NULL) return OS_FS_ERR_INVALID_POINTER;
    if (strlen(path) > OS_MAX_PATH_LEN) return OS_FS_ERR_PATH_TOO_LONG;
    if (path[0] != '/') return OS_FS_ERR_PATH_INVALID;
    if (file_find((char *)path) == NULL) return OS_FS_ERR_PATH_INVALID;
    return 0;
}

// chk_path_new is for files that should not already be in the filesystem
static inline int32 chk_path_new(const char *path)
{
    if (path == NULL) return OS_FS_ERR_INVALID_POINTER;
    if (strlen(path) > OS_MAX_PATH_LEN) return OS_FS_ERR_PATH_TOO_LONG;
    return OS_FS_SUCCESS;
}

uint32 fs_mount(char *devname, char *mountpoint);

uint32 fs_unmount(char *mountpoint);

uint32 fs_init(struct fs *filesys, char *devname, char *volname, uint32 blocksize, uint32 numblocks);

uint32 newfs_init(char *defilvname, char *volname, uint32 blocksize, uint32 numblocks);

int32 rmfs(char *devname);

int32 filesys_GetPhysDriveName(char *PhysDriveName, char *MountPoint);

int32 filesys_GetFsInfo(os_fsinfo_t *finesys_info);

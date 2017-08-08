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
#define MAX_NUM_FS NUM_TABLE_ENTRIES
#define MAX_NUM_FILES 100
#define MAX_NUM_DIRENT 10

// a page is 4096, size of f_part is 5 values * 4 bytes
#define F_PART_DATA_SIZE (4096-sizeof(struct f_part))

// currently we compile composite and cFE with different libc
// muslc for cos and whatever ubuntu32 14 use for cFE
// these have different structs so I need to implement a dirent struct in cos
// with the same layout as cFE's dirent
struct hack_dirent
{
    ino_t d_ino;
    // off_t d_off; these are the 8 bytes that I am removing
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
};

// for exactly the same reason as above, we need to declare a stat structure to pass
// back to the cFE that it will recoginize
struct hack_stat
{
    int64 st_dev;
    uint32 padding1; // here to keep the offset consistant
    int32 st_ino;
    int32 st_mode;
    int32 st_nlink;
    int32 st_gid;
    int32 st_uid;
    int64 st_rdev;
    uint32 padding2; // also here to keep offset consistant with cFE
    int32 st_size;
    int32 st_blksize;
    int32 st_blocks;
    int32 st_hack_atime; // st_time field are defined elsewhere
    int32 st_hack_mtime;
    int32 st_hack_ctime;
};

typedef enum {
    FSOBJ_FILE,
    FSOBJ_DIR,
} fsobj_type; //TODO: what is the preferred naming convention for this

typedef enum {
    READ,
    WRITE,
    READ_WRITE,
    NONE,
} permission_t;

struct file_position {
    struct f_part *open_part;  //part of file being read/written to
    uint32 part_offset;         //offset into part's data
    uint32 file_offset;         //position within file as a whole
};

typedef enum {
    STATIC,
    DYNAMIC,
} fpart_alloc_t;

/*
 * The structure of the filesystem is stored in the fsobj struct
 * each non-leaf dir points to its first child, which then has a pointer
 * to the next, etc. Each file has a linked list of f_parts to store blocks of data
 */

struct fsobj {
    char *name;
    int32 ino; // 0 for free file
    fsobj_type type; /* dir vs file, determines the type of FD position */
    size_t size;
    uint32 refcnt; //number of filedes which have it opened
    permission_t permission; // most permissive possible status it may be opened with
    fpart_alloc_t memtype;
    struct f_part *file_part;
    struct fsobj *next, *prev;
    struct fsobj *child, *parent; // child != NULL iff type = dir
};

struct f_part {
    struct fsobj *file;
    struct f_part *next, *prev;
    char *data;
};

/*
  * The state of a directory stream is either iterating through list of files,
  * at one of two special paths, or at the end of the stream.  The two special
  * paths are '.' and '..' and the occur at the end of the list of files
 */
typedef enum {
    NORMAL,
    CUR_DIR_LINK,
    PARENT_DIR_LINK,
    END_OF_STREAM,
} dir_stream_status_t;

// offset into linked list of children
struct dir_position {
    struct fsobj *open_dir;     /* Stream is children of open_dir */
    struct fsobj *cur;          /* refers to current (last returned) file in dir stream */
    dir_stream_status_t status; /* indicates if special file or end of stream */
    struct hack_dirent dirent;         /* I really don't like storing this here. */
};

/*
 * The type being used must be consistent with fsobj->type
 */
union fd_position {
    struct dir_position dir_pos;
    struct file_position file_pos;
};

// Currently this filedes is used only for dir streams, and not real filedes
// TODO: Switch non-dir to table based filedes
struct fd {
    int32 ino;
    permission_t access;  /* must be < or == permissive as file->permission */
    union fd_position position; /* the type of position is determined by file->type */
    struct fsobj *file;
};

struct fs {
    char *devname;
    char *volname;
    char *mountpoint;
    uint32 blocksize;
    uint32 numblocks;
    struct fsobj *root;
};

static char *path_to_name(char *path)
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
int32 file_rm(struct fsobj *o);

int32 file_close_by_ino(int32 ino);

int32 file_close_by_name(char *path);

uint32 newpart_get(struct f_part **part);

uint32 newfile_get(struct fsobj **o);

uint32 file_insert(struct fsobj *o, char *path);

int32 file_open(char *path, permission_t permission);

int32 file_close(int32 filedes);

int32 file_read(int32 FD, void *buffer, uint32 nbytes);

int32 file_write(int32 FD, void *buffer, uint32 nbytes);

struct fsobj *file_find(char *path);

int32 file_create(char *path, permission_t permission);

int32 file_remove(char *path);

int32 file_rename(char *old_filename, char *new_filename);

int32 file_cp(char *src, char *dest);

int32 file_mv(char *src, char *dest);

int32 chk_fd(int32 FD);

int32 file_FDGetInfo(int32 FD, OS_FDTableEntry *fd_prop);

int32 file_stat(char *path, struct hack_stat  *filestats);

int32 file_lseek(int32 FD, int32 offset, uint32 whence);

permission_t permission_cFE_to_COS(uint32 permission);

uint32 permission_COS_to_cFE(permission_t permission);

/******************************************************************************
** dirent Level Methods
******************************************************************************/
int32 newfd_get(int32 ino);

int32 dir_open(char *path);

uint32 dir_close(int32 FD);

void dir_rewind(int32 FD);

os_dirent_t *dir_read(int32 FD);

int32 file_mkdir(char *path);

int32 file_rmdir(char *path);

/******************************************************************************
** fs Level Methods
******************************************************************************/
int32 path_chk_isvalid(const char *path);

int32 path_chk_exists(const char *path);

int32 path_translate(char *virt, char *local);

uint32 fs_mount(char *devname, char *mountpoint);

uint32 fs_unmount(char *mountpoint);

uint32 fs_init(struct fs *filesys, char *devname, char *volname, uint32 blocksize, uint32 numblocks);

uint32 newfs_init(char *defilvname, char *volname, uint32 blocksize, uint32 numblocks);

int32 rmfs(char *devname);

int32 filesys_GetPhysDriveName(char *PhysDriveName, char *MountPoint);

int32 filesys_GetFsInfo(os_fsinfo_t *finesys_info);

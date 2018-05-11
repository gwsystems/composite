#include <string.h>
#include <dirent.h>

#include <cos_component.h>
#include <cos_defkernel_api.h>
#include <cos_kernel_api.h>

#include <memmgr.h>

#include "gen/common_types.h"
#include "gen/osapi.h"
#include "gen/osapi-os-filesys.h"

#include "cFE_util.h"

/* Not to be confused with similar OSAL constants.
 * These are only to define size of statically allocated data
 */
#define MAX_NUM_FS NUM_TABLE_ENTRIES
#define MAX_NUM_FILES 100
#define MAX_NUM_DIRENT 10

/* a page is 4096, size of f_part is 5 values * 4 bytes */
#define F_PART_DATA_SIZE (4096 - sizeof(struct f_part))

enum fsobj_type
{
	FSOBJ_FILE,
	FSOBJ_DIR,
};

enum fs_permissions
{
	NONE    = 0,
	READ    = 0x001,
	WRITE   = 0x010,
	EXECUTE = 0x100,
	ALL     = READ | WRITE | EXECUTE,
};

struct file_position {
	struct f_part *open_part;   // part of file being read/written to
	uint32         part_offset; // offset into part's data
	uint32         file_offset; // position within file as a whole
};

enum fpart_alloc_type
{
	STATIC,
	DYNAMIC,
};

/*
 * The structure of the filesystem is stored in the fsobj struct
 * each non-leaf dir points to its first child, which then has a pointer
 * to the next, etc. Each file has a linked list of f_parts to store blocks of data
 */

struct fsobj {
	char *                name;
	int32                 ino;  // 0 for free file
	enum fsobj_type       type; /* dir vs file, determines the type of FD position */
	size_t                size;
	uint32                refcnt;     // number of filedes which have it opened
	enum fs_permissions   permission; // most permissive possible status it may be opened with
	enum fpart_alloc_type memtype;
	struct f_part *       file_part;
	struct fsobj *        next, *prev;
	struct fsobj *        child, *parent; // child != NULL iff type = dir
};

struct f_part {
	struct fsobj * file;
	struct f_part *next, *prev;
	char *         data;
};

/*
 * The state of a directory stream is either iterating through list of files,
 * at one of two special paths, or at the end of the stream.  The two special
 * paths are '.' and '..' and the occur at the end of the list of files
 */
enum dir_stream_status
{
	NORMAL,
	CUR_DIR_LINK,
	PARENT_DIR_LINK,
	END_OF_STREAM,
};

// offset into linked list of children
struct dir_position {
	struct fsobj *         open_dir; /* Stream is children of open_dir */
	struct fsobj *         cur;      /* refers to current (last returned) file in dir stream */
	enum dir_stream_status status;   /* indicates if special file or end of stream */
	os_dirent_t            dirent;   /* I really don't like storing this here. */
};

/*
 * The type being used must be consistent with fsobj->type
 */
union fd_position {
	struct dir_position  dir_pos;
	struct file_position file_pos;
};

// Currently this filedes is used only for dir streams, and not real filedes
// TODO: Switch non-dir to table based filedes
struct fd {
	int32               ino;
	enum fs_permissions access;   /* must be < or == permissive as file->permission */
	union fd_position   position; /* the type of position is determined by file->type */
	struct fsobj *      file;
};

struct fs {
	char *        devname;
	char *        volname;
	char *        mountpoint;
	uint32        blocksize;
	uint32        numblocks;
	struct fsobj *root;
};

static char *
path_to_name(char *path)
{
	uint32 path_len, offset;

	assert(path);
	path_len = strlen(path);
	assert(path_len > 1);

	// remove one or more '/' at the end of path
	while (path[strlen(path) - 1] == '/') { path[strlen(path) - 1] = 0; }

	// iterate from right to left through the path until you find a '/'
	// everything you have iterated through is the name of the file
	for (offset = path_len - 2; path[offset] != '/' && offset > 0; offset--) {
		// do nothing
	}
	char *name = path + offset + 1;

	assert(0 < strlen(name));
	return name;
}

/******************************************************************************
** fsobj Level Methods
******************************************************************************/
int32 file_rm(struct fsobj *o);

int32 file_close_by_ino(int32 ino);

int32 file_close_by_name(char *path);

uint32 part_get_new(struct f_part **part);

uint32 file_get_new(struct fsobj **o);

uint32 file_insert(struct fsobj *o, char *path);

int32 file_open(char *path, enum fs_permissions permission);

int32 file_close(int32 filedes);

int32 file_read(int32 FD, void *buffer, uint32 nbytes);

int32 file_write(int32 FD, void *buffer, uint32 nbytes);

struct fsobj *file_find(char *path);

int32 file_create(char *path, enum fs_permissions permission);

int32 file_remove(char *path);

int32 file_rename(char *old_filename, char *new_filename);

int32 file_cp(char *src, char *dest);

int32 file_mv(char *src, char *dest);

int32 chk_fd(int32 FD);

int32 file_FDGetInfo(int32 FD, OS_FDTableEntry *fd_prop);

int32 file_stat(char *path, os_fstat_t *filestats);

int32 file_lseek(int32 FD, int32 offset, uint32 whence);

enum fs_permissions permission_cFE_to_COS(uint32 permission);

uint32 permission_COS_to_cFE(enum fs_permissions permission);

/******************************************************************************
** dirent Level Methods
******************************************************************************/
// int32 newfd_get(int32 ino);

int32 dir_open(char *path);

uint32 dir_close(int32 FD);

void dir_rewind(int32 FD);

os_dirent_t *dir_read(int32 FD);

int32 file_mkdir(char *path);

int32 file_rmdir(char *path);

/******************************************************************************
** fs Level Methods
******************************************************************************/
int32 path_isvalid(const char *path);

int32 path_exists(const char *path);

int32 path_translate(char *virt, char *local);

uint32 fs_mount(char *devname, char *mountpoint);

uint32 fs_unmount(char *mountpoint);

uint32 fs_init(char *defilvname, char *volname, uint32 blocksize, uint32 numblocks);

int32 fs_remove(char *devname);

int32 fs_get_drive_name(char *PhysDriveName, char *MountPoint);

int32 fs_get_info(os_fsinfo_t *finesys_info);

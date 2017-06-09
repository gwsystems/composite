#include "cFE_util.h"
#include "osfilesys.h"

struct FS fs[MAX_NUM_FS];
struct fsobj files[MAX_NUM_FILES];

/*
 * Notes on this version:
 * This is not the final implementation of a file system for the cFE on COS
 * For now the file system is read only and staticly allocates for fsobj and FS
 * The next step is to write a dumb allocator/deallocator and use it for
 * new fs, fsobj, and file data
 * I have been intentional to restrict memory specific details to this file
 * osfilesys.h and osfiles.c 'should' not care about how FS, fsobj, and data
 * are stored in memory.
 */

/******************************************************************************
** Tar Level Methods
******************************************************************************/

/*
 * Loads the position in memory of linked tar file system
 * Checks for badly linked or no linked tar file.  The names given by
 * the linker are non-intuative so a description of error checking is given:
 * First checks to make sure that symbols have been overwritten by linking
 * process.  Next checks that the size is greater than 0.  Finally checks that
 * the end of the tar is after the start
 */
uint32 tar_load()
{
    if (!_binary_cFEfs_tar_start)
        return OS_FS_ERR_DRIVE_NOT_CREATED;
    if (! &_binary_cFEfs_tar_size)
        return OS_FS_ERR_DRIVE_NOT_CREATED;
    if (&_binary_cFEfs_tar_end < &_binary_cFEfs_tar_start)
        return OS_FS_ERR_DRIVE_NOT_CREATED;
    size    = (size_t) &_binary_cFEfs_tar_size;
    start   = &_binary_cFEfs_tar_start;
    end     = &_binary_cFEfs_tar_end;
    return  OS_FS_SUCCESS;
}

/*
 * parses a loaded tar into fs[0]
 */
uint32 tar_parse()
{
    assert(start && end);
    assert(size < INT32_MAX);
    assert(end - start > 0);
    assert(size == (size_t) (end - start));
    assert(fs[0].root->filedes == 1); //first fs should be rooted by first file

    if (!fs[0].devname) return OS_FS_ERR_DRIVE_NOT_CREATED;

    struct fsobj *o = fs[0].root;
    uint32 offset = 0;

    while (offset + start < end) {
        //tar ends after two empty records
        if ( !(offset + start)[0] && !(offset + start)[TAR_BLOCKSIZE]) return OS_FS_SUCCESS;

        if (tar_cphdr(offset, o))     return OS_FS_ERR_DRIVE_NOT_CREATED;
        if (file_insert(o, &fs[0]))   return OS_FS_ERR_DRIVE_NOT_CREATED;
        //data is alligned to 512 byte blocks.  a header is 500 bytes, and
        // the file's data begins exactly after the header
        // therefor the next header is 500 + o->size rounded up to a mult of 512
        offset += round_to_blocksize(o->size + 500);
        if (newfile_get(&o))         return OS_FS_ERR_DRIVE_NOT_CREATED;
    }

    //tar ends before two empty records are found
    return OS_FS_ERROR;
}

/*
 * Copies information from a tar file header to a fsobj
 */
uint32 tar_cphdr(uint32 tar_offset, struct fsobj *file)
{
    //TODO: store filenames without paths
    assert(tar_offset < size);
    assert(file->filedes > 0);

    char *location = start;
    location += tar_offset;
    file->name          = location;
    if (*(location + strlen(location) - 1) == '/') {
        file->type      = FSOBJ_DIR;
        file->size      = 0;
    }
    else{
        file->type      = FSOBJ_FILE;
        file->size      = oct_to_dec(location + 124);
        file->data      = location + 500;
    }
    return OS_FS_SUCCESS;
}

//this could be more performant bithack
uint32 round_to_blocksize(uint32 offset)
{
    if (offset % TAR_BLOCKSIZE) return offset + (TAR_BLOCKSIZE - (offset % TAR_BLOCKSIZE));
    return offset;
}

/******************************************************************************
** fsobj Level Methods
******************************************************************************/

uint32 fsobj_init(struct fsobj *o)
{
    assert(o);
    assert(o->filedes > 0);

    o->name     = "EXAMPLE_NAME";
    o->type     = FSOBJ_DIR;
    o->size     = 0;
    o->refcnt   = 0;
    o->data     = NULL;
    o->child    = o->parent = NULL;
    o->next     = o->prev = NULL;
    return OS_FS_SUCCESS;
}

//finds the next open file
uint32 newfile_get(struct fsobj **o)
{
    uint32 count = 0;
    while (count < MAX_NUM_FILES && files[count].filedes) {
        count++;
    }
    if (count == MAX_NUM_FILES) return OS_FS_ERROR;
    *o =  &files[count];
    (*o)->filedes = count + 1;
    //filedes needs to be unique and nonzero, so filedes is defined as index+1
    return OS_FS_SUCCESS;
}

uint32 file_insert(struct fsobj *o, struct FS *filesys)
{
    assert(filesys && filesys->root && filesys->root->filedes);
    if (filesys->root == o) return OS_FS_SUCCESS;

    insert(o, filesys->root);

    return OS_FS_SUCCESS;
}

//could be written iteratively to use less stackframe if that becomes an issue
// Root is
uint32 insert(struct fsobj *o, struct fsobj *root)
{
    assert(root && o);
    assert(strlen(root->name) < strlen(o->name));

    //loop terminates when it finds a place to put o, either at the end of a
    //list of children or as the first in an empty children list
    while (1) {
        assert(strcmp(root->name, o->name) !=0 );
        assert(strspn(o->name, root->name) >= strlen(root->name));
        //if there is no child, then insert as child
        if (!root->child) {
            o->parent = root;
            root->child = o;
            return OS_FS_SUCCESS;
        }
        root = root->child;

        //precondition: root is the first in a non-empty list of children
        //postcondition: root is an ancestor of o
        while (strspn(o->name, root->name) < strlen(root->name)) {
            if (!root->next) {
                root->next = o;
                o->prev = root;
                o->parent = root->parent;
                return OS_FS_SUCCESS;
            }
            root = root->next;
        }

    }
    PANIC("Unreachable Statement");
    return 0;
}

int32 open(const char *path) {
    assert(fs[0].root && !fs[1].root);
    struct fsobj *root = fs[0].root;
    root = find(path, root);
    if (!root) return OS_FS_ERROR;
    root->refcnt++;
    return root->filedes;
}

int32 close(int32 filedes) {
    uint32 index = filedes + 1;
    if (files[index].filedes != filedes)   return OS_FS_ERROR;
    if (files[index].refcnt < 1)           return OS_FS_ERROR;
    files[index].refcnt--;
    return OS_FS_SUCCESS;
}

int32 read(int32  filedes, void *buffer, uint32 nbytes) {
    if (!buffer)                                 return OS_FS_ERR_INVALID_POINTER;
    if (files[filedes + 1].filedes != filedes)   return OS_FS_ERROR;
    if (files[filedes + 1].refcnt < 1)           return OS_FS_ERROR;
    if (nbytes == 0)                             return 0;
    memcpy(buffer, files[filedes + 1].data, nbytes);
    return nbytes;
}

struct fsobj *find(const char *path, struct fsobj *root)
{
    assert(root);
    assert(strlen(root->name) < strlen(path));
    while (1) {
        assert(strcmp(root->name, path) !=0 );
        assert(strspn(path, root->name) >= strlen(root->name));
        if (!root->child) return NULL;

        root = root->child;

        while (strspn(path, root->name) < strlen(root->name)) {
            if (!strcmp(root->name, path)) return root;
            if (!root->next) return NULL;
            root = root->next;
        }

    }
    PANIC("Unreachable Statement");
    return NULL;
}

/******************************************************************************
** fs Level Methods
******************************************************************************/

uint32 mount(const char *devname, char *mountpoint)
{
    uint32 i;
    for (i=0 ; i < MAX_NUM_FS && fs[i].devname != NULL ; i++) {
        if (!strcmp(fs[i].devname, devname)) {
            fs[i].mountpoint = mountpoint;
            return OS_FS_SUCCESS;
        }
    }
    return OS_FS_ERROR;
}

uint32 unmount(const char *mountpoint) {
    uint32 i;
    for (i=0 ; i < MAX_NUM_FS ; i++) {
        if (mountpoint != NULL && !strcmp(fs[i].mountpoint, mountpoint)) {
            fs[i].mountpoint = NULL;
            return OS_FS_SUCCESS;
        }
    }
    return OS_FS_ERROR;
}

/*
 * Given a pointer to an FS, inits values and fsobj for root
 */
uint32 fs_init(struct FS *filesys, char *devname, char *volname, uint32 blocksize, uint32 numblocks)
{
    struct fsobj *o         = NULL;
    if (newfile_get(&o))    return OS_FS_ERROR;
    if (fsobj_init(o))      return OS_FS_ERROR;
    filesys->devname        = devname;
    filesys->mountpoint     = "";
    filesys->blocksize      = blocksize;
    filesys->numblocks      = numblocks;
    filesys->root           = o;
    filesys->next           = NULL;
    filesys->prev           = NULL;
    return OS_FS_SUCCESS;
}

uint32 newfs_init(char *devname, char *volname, uint32 blocksize, uint32 numblocks)
{
    uint32 count = 1, ret = 0;
    if (!devname)                           return OS_FS_ERR_INVALID_POINTER;
    if (blocksize == 0 || numblocks == 0)   return OS_FS_ERROR;
    if (strlen(devname) >= OS_FS_DEV_NAME_LEN || strlen(volname) >= OS_FS_VOL_NAME_LEN)
        return OS_FS_ERROR;

    //the first filesystem is always the tar
    if (!fs[0].devname) {
        ret = tar_load();
        if (ret != OS_FS_SUCCESS) return ret;
        ret = fs_init(&fs[0], devname, volname, blocksize, numblocks);
        if (ret != OS_FS_SUCCESS) return ret;
        return OS_FS_SUCCESS;
    }

    while (count < MAX_NUM_FS && fs[count].devname) {
        count++;
    }
    if (count == MAX_NUM_FS)
        return OS_FS_ERR_DRIVE_NOT_CREATED;

    ret = fs_init(&fs[count], devname, volname, blocksize, numblocks);
    if (ret !=OS_FS_SUCCESS) return ret;

    return OS_FS_SUCCESS;
}

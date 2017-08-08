#include "osfilesys.h"
#include "tar.h"
#include <string.h>

//should be overwritten by linking step in build process
__attribute__((weak)) char _binary_cFEfs_tar_size=0;
__attribute__((weak)) char _binary_cFEfs_tar_start=0;
__attribute__((weak)) char _binary_cFEfs_tar_end=0;

//locations and size of tar
char      *tar_start;
char      *tar_end;
size_t    tar_size;

uint32 round_to_blocksize(uint32 offset)
{
    if (offset % TAR_BLOCKSIZE) return offset + (TAR_BLOCKSIZE - (offset % TAR_BLOCKSIZE));
    return offset;
}

//used to convert filesize in oct to dec
uint32 oct_to_dec(char *oct)
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


/*
 * Loads the position in memory of linked tar file system
 * Checks for badly linked or no linked tar file.  The names given by
 * the linker are non-intuitive so a description of error checking is given:
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

    tar_size    = (size_t) &_binary_cFEfs_tar_size;
    tar_start   = &_binary_cFEfs_tar_start;
    tar_end     = &_binary_cFEfs_tar_end;

    return  OS_FS_SUCCESS;
}


/*
 * parses a loaded tar into whichever filesystem is mounted
 * precondition: tar has been loaded by tar_load and init by newfs_init
 * Postcondition: A proper error code is returned OR the tar is represented in memory
 * at the currently open filesystem
 */
uint32 tar_parse()
{
    assert(tar_start && tar_end);
    assert(tar_size < INT32_MAX);
    assert(tar_end - tar_start > 0);
    assert(tar_size == (size_t) (tar_end - tar_start));
    uint32 offset = 0;
    struct fsobj *o;


    while (offset + tar_start < tar_end) {
        if (newfile_get(&o))            return OS_FS_ERR_DRIVE_NOT_CREATED;

        //tar ends after two empty records
        if ( !(offset + tar_start)[0] && !(offset + tar_start)[TAR_BLOCKSIZE]) {
            o->ino = 0;
            return OS_FS_SUCCESS;
        }
        if (tar_cphdr(offset, o))           return OS_FS_ERR_DRIVE_NOT_CREATED;
        if (file_insert(o, offset + tar_start)) return OS_FS_ERR_DRIVE_NOT_CREATED;

        /*
         * data is aligned to 512 byte blocks.  a header is 500 bytes, and
         * the file's data begins exactly after the header
         * therefor the next header is 500 + o->size rounded up to a mult of 512
         */
         offset += round_to_blocksize(o->size + 500);
    }
    //tar ends before two empty records are found
    return OS_FS_ERROR;
}

/*
 * Copies information from a tar file header to a fsobj
 * TODO: change name of this function and make it cast to tar hdr struct
 */
uint32 tar_cphdr(uint32 tar_offset, struct fsobj *file)
{
    assert(tar_offset < tar_size);
    assert(file->ino > 0);
    struct f_part *part;
    newpart_get(&part);
    file->memtype = STATIC;
    char *location = tar_start;
    location += tar_offset;
    memmove(location + 1, location, strlen(location));
    location[0] = '/';
    file->name = path_to_name(location);
    if (*(location + strlen(location) - 1) == '/') {
        file->type              = FSOBJ_DIR;
        file->size              = 0;
    }
    else{
        file->type              = FSOBJ_FILE;
        file->size              = oct_to_dec(location + 124);
        file->file_part         = part;
        file->file_part->data   = location + 500;
    }
    return OS_FS_SUCCESS;
}


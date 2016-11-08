#include "cFE_util.h"

#include "gen/osapi.h"
#include "gen/common_types.h"


/******************************************************************************
** Standard File system API
******************************************************************************/
/*
 * Initializes the File System functions
*/

int32 OS_FS_Init(void)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Creates a file specified by path
*/
int32 OS_creat(const char *path, int32  access)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Opend a file for reading/writing. Returns file descriptor
*/
int32 OS_open(const char *path,  int32 access,  uint32 mode)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Closes an open file.
*/
int32 OS_close(int32  filedes)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Reads nbytes bytes from file into buffer
*/
int32 OS_read(int32  filedes, void *buffer, uint32 nbytes)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Write nybytes bytes of buffer into the file
*/
int32 OS_write(int32  filedes, void *buffer, uint32 nbytes)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Changes the permissions of a file
*/
int32 OS_chmod(const char *path, uint32 access)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Returns file status information in filestats
*/
int32 OS_stat(const char *path, os_fstat_t  *filestats)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Seeks to the specified position of an open file
*/
int32 OS_lseek(int32  filedes, int32 offset, uint32 whence)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Removes a file from the file system
*/
int32 OS_remove(const char *path)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Renames a file in the file system
*/
int32 OS_rename(const char *old_filename, const char *new_filename)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * copies a single file from src to dest
*/
int32 OS_cp(const char *src, const char *dest)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * moves a single file from src to dest
*/
int32 OS_mv(const char *src, const char *dest)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Copies the info of an open file to the structure
*/
int32 OS_FDGetInfo(int32 filedes, OS_FDTableEntry *fd_prop)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** Check to see if a file is open
*/
int32 OS_FileOpenCheck(char *Filename)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** Close all open files
*/
int32 OS_CloseAllFiles(void)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** Close a file by filename
*/
int32 OS_CloseFileByName(char *Filename)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}


/******************************************************************************
** Directory API
******************************************************************************/

/*
 * Makes a new directory
*/
int32 OS_mkdir(const char *path, uint32 access)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Opens a directory for searching
*/
os_dirp_t OS_opendir(const char *path)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Closes an open directory
*/
int32 OS_closedir(os_dirp_t directory)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Rewinds an open directory
*/
void OS_rewinddir(os_dirp_t directory)
{
    panic("Unimplemented method!"); // TODO: Implement me!
}

/*
 * Reads the next object in the directory
*/
os_dirent_t * OS_readdir(os_dirp_t directory)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Removes an empty directory from the file system.
*/
int32 OS_rmdir(const char *path)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/******************************************************************************
** System Level API
******************************************************************************/
/*
 * Makes a file system
*/
int32 OS_mkfs(char *address,char *devname, char *volname,
                                uint32 blocksize, uint32 numblocks)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}
/*
 * Mounts a file system
*/
int32 OS_mount(const char *devname, char *mountpoint)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Initializes an existing file system
*/
int32 OS_initfs(char *address,char *devname, char *volname, uint32 blocksize, uint32 numblocks)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * removes a file system
*/
int32 OS_rmfs(char *devname)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Unmounts a mounted file system
*/
int32 OS_unmount(const char *mountpoint)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Returns the number of free blocks in a file system
*/
int32 OS_fsBlocksFree(const char *name)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** Returns the number of free bytes in a file system
** Note the 64 bit data type to support filesystems that
** are greater than 4 Gigabytes
*/
int32 OS_fsBytesFree(const char *name, uint64 *bytes_free)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Checks the health of a file system and repairs it if neccesary
*/
os_fshealth_t OS_chkfs(const char *name, boolean repair)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
 * Returns in the parameter the physical drive underneith the mount point
*/
int32 OS_FS_GetPhysDriveName(char * PhysDriveName, char * MountPoint)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** Translates a OSAL Virtual file system path to a host Local path
*/
int32 OS_TranslatePath(const char *VirtualPath, char *LocalPath)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
**  Returns information about the file system in an os_fsinfo_t
*/
int32 OS_GetFsInfo(os_fsinfo_t  *filesys_info)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/******************************************************************************
** Shell API
******************************************************************************/

/* executes the shell command passed into is and writes the output of that
 * command to the file specified by the given OSAPI file descriptor */
int32 OS_ShellOutputToFile(char* Cmd, int32 OS_fd)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

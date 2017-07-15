#include "osfilesys.h"

/******************************************************************************
** Standard File system API
******************************************************************************/
/*
 * Initializes the File System functions
*/

int32 OS_FS_Init(void)
{
    uint32 ret = 0;
    ret = newfs_init("/ramdev0", "RAM", 512, 4096);
    if (ret != OS_FS_SUCCESS)   return ret;
    ret = tar_parse();
    if (ret != OS_FS_SUCCESS)   return ret;
    return OS_FS_SUCCESS;
}

/*
 * Creates a file specified by path
*/
int32 OS_creat(const char *path, int32  access)
{
    int32 ret = chk_path_new(path);
    if (ret) return ret;
    file_creat(path);
    return 0;
}

/*
 * Opend a file for reading/writing. Returns file descriptor
*/
int32 OS_open(const char *path,  int32 access,  uint32 mode)
{
    int32 ret = chk_path(path);
    if (ret) return ret;
    return file_open(path);
}

/*
 * Closes an open file.
*/
int32 OS_close(int32 filedes)
{
    return file_close(filedes);
}

/*
 * Reads nbytes bytes from file into buffer
*/
int32 OS_read(int32 filedes, void *buffer, uint32 nbytes)
{
    return file_read(filedes, buffer, nbytes);
}

/*
 * Write nybytes bytes of buffer into the file
*/
int32 OS_write(int32  filedes, void *buffer, uint32 nbytes)
{
    return file_write(filedes, buffer, nbytes);
}

/*
 * Changes the permissions of a file
 * This is not used by the cFE and UT tests that it returns NOT_IMPLEMENTED
*/
int32 OS_chmod(const char *path, uint32 access)
{
    return OS_ERR_NOT_IMPLEMENTED;
}

/*
 * Returns file status information in filestats
*/
int32 OS_stat(const char *path, os_fstat_t  *filestats)
{
    if (filestats == NULL) return OS_FS_ERR_INVALID_POINTER;
    int32 ret = chk_path(path);
    if (ret) return ret;
    return file_stat(path, filestats);
}

/*
 * Seeks to the specified position of an open file
*/
int32 OS_lseek(int32  filedes, int32 offset, uint32 whence)
{
    return file_lseek(filedes, offset, whence);
}

/*
 * Removes a file from the file system
*/
int32 OS_remove(const char *path)
{
    int32 ret = chk_path(path);
    if (ret) return ret;
    file_remove(path);
    return 0;
}

/*
 * Renames a file in the file system
*/
int32 OS_rename(const char *old_filename, const char *new_filename)
{
    int32 ret = chk_path(old_filename);
    if (ret) return ret;
    ret = chk_path(new_filename);
    if (ret) return ret;
    return file_rename(old_filename, new_filename);
}

/*
 * copies a single file from src to dest
*/
int32 OS_cp(const char *src, const char *dest)
{
    if (!src || !dest) return OS_FS_ERR_INVALID_POINTER;
    if (strlen((char *)src) > OS_MAX_PATH_LEN) return OS_FS_ERR_PATH_TOO_LONG;
    if (strlen((char *)dest) > OS_MAX_PATH_LEN) return OS_FS_ERR_PATH_TOO_LONG;
    if (strlen(path_to_name((char *)src)) > OS_MAX_FILE_NAME) return OS_FS_ERR_NAME_TOO_LONG;
    if (strlen(path_to_name((char *)dest)) > OS_MAX_FILE_NAME) return OS_FS_ERR_NAME_TOO_LONG;
    int32 ret = chk_path(src);
    if (ret) return ret;
    ret = chk_path_new(dest);
    if (ret) return ret;
    return file_cp(src, dest);
}

/*
 * moves a single file from src to dest
*/
int32 OS_mv(const char *src, const char *dest)
{
    if (!src || !dest) return OS_FS_ERR_INVALID_POINTER;
    if (strlen((char *)src) > OS_MAX_PATH_LEN) return OS_FS_ERR_PATH_TOO_LONG;
    if (strlen((char *)dest) > OS_MAX_PATH_LEN) return OS_FS_ERR_PATH_TOO_LONG;
    if (strlen(path_to_name((char *)src)) > OS_MAX_FILE_NAME) return OS_FS_ERR_NAME_TOO_LONG;
    if (strlen(path_to_name((char *)dest)) > OS_MAX_FILE_NAME) return OS_FS_ERR_NAME_TOO_LONG;
    int32 ret = chk_path(src);
    if (ret) return ret;
    ret = chk_path_new(dest);
    if (ret) return ret;
    return file_mv(src, dest);
}

/*
 * Copies the info of an open file to the structure
*/
int32 OS_FDGetInfo(int32 filedes, OS_FDTableEntry *fd_prop)
{
    if (fd_prop == NULL)                            return OS_FS_ERR_INVALID_POINTER;
    if (filedes <= 0 || filedes > MAX_NUM_FILES)    return OS_FS_ERR_INVALID_FD;

    return file_FDGetInfo(filedes, fd_prop);
}

/*
** Check to see if a file is open
*/
int32 OS_FileOpenCheck(char *Filename)
{
    int32 ret = chk_path(Filename);
    if (ret) return OS_FS_ERR_INVALID_POINTER;

    struct fsobj *file = file_find(Filename);
    if (file == NULL) return OS_INVALID_POINTER;
    if (file->refcnt == 0) return OS_FS_ERROR;
    return OS_FS_SUCCESS;

}

/*
** Close all open files
*/
int32 OS_CloseAllFiles(void)
{
    uint32 i;
    for (i = 1 ; i < MAX_NUM_FILES + 1 ; i++) {
        OS_close(i);
    }
    return OS_FS_SUCCESS;
}

/*
** Close a file by filename
*/
int32 OS_CloseFileByName(char *Filename)
{
    int32 ret = chk_path(Filename);
    if (ret) return OS_FS_ERR_INVALID_POINTER;

    struct fsobj *file = file_find(Filename);
    if (file == NULL || file->filedes == 0) return OS_FS_ERR_INVALID_POINTER;
    return OS_close(file->filedes);
}

/******************************************************************************
** Directory API
******************************************************************************/

/*
 * Makes a new directory
 * access is not used by cFE
*/
int32 OS_mkdir(const char *path, uint32 access)
{
    int32 ret = chk_path_new(path);
    if (ret) return ret;
    return file_mkdir(path, access);
}

/*
 * Opens a directory for searching
 * TODO: Check for error codes conflicting with real values
*/
os_dirp_t OS_opendir(const char *path)
{
    if(chk_path(path) != OS_FS_SUCCESS) return NULL;
    int32 filedes = file_open(path);
    if (filedes == OS_FS_ERROR) return NULL;;
    return (os_dirp_t) file_open(path);
}

/*
 * Closes an open directory
*/
int32 OS_closedir(os_dirp_t directory)
{
    if (directory == NULL) return OS_FS_ERR_INVALID_POINTER;
    return file_close((int32) directory);
}

/*
 * Rewinds an open directory
*/
void OS_rewinddir(os_dirp_t directory)
{
    file_rewinddir();
    return;
}

/*
 * Reads the next object in the directory
*/
os_dirent_t * OS_readdir(os_dirp_t directory)
{
    if (directory == NULL) return NULL;
    return file_readdir( (int32) directory);
}

/*
 * Removes an empty directory from the file system.
*/
int32 OS_rmdir(const char *path)
{
    int32 ret = chk_path(path);
    if (ret) return ret;
    return file_rmdir(path);
}

/******************************************************************************
** System Level API
******************************************************************************/
/*
 * Makes a file system
*/
int32 OS_mkfs(char *address, char *devname, char *volname,
                                uint32 blocksize, uint32 numblocks)
{
    if (address)                return OS_FS_ERR_INVALID_POINTER;
    if (!devname || !volname)   return OS_FS_ERR_INVALID_POINTER;
    if (strlen(devname) >= OS_FS_DEV_NAME_LEN || strlen(volname) >= OS_FS_VOL_NAME_LEN)
                                return OS_FS_ERR_PATH_TOO_LONG;
    return newfs_init(devname, volname, blocksize, numblocks);
}
/*
 * Mounts a file system
*/
int32 OS_mount(const char *devname, char *mountpoint)
{
    if (!devname || !mountpoint) return OS_FS_ERR_INVALID_POINTER;
    return fs_mount(devname, mountpoint);
}

/*
 * Initializes an existing filesystem
 * address will be null if wants to initialize an empty fs, non-null to load an fs from memory
 * we could easily load a tar from memory but if an application wants to load a filesystem it
 * is safer to panic as we do not know what format the application is attempting to load
*/
int32 OS_initfs(char *address, char *devname, char *volname, uint32 blocksize, uint32 numblocks)
{
    if (address) PANIC("No support for loading a filesystem from arbitrary memory");
    return OS_FS_SUCCESS;
}

/*
 * removes a file system
*/
int32 OS_rmfs(char *devname)
{
    if (devname == NULL) return OS_FS_ERR_INVALID_POINTER;
    return rmfs(devname);
}

/*
 * Unmounts a mounted file system
*/
int32 OS_unmount(const char *mountpoint)
{
    if (!mountpoint) return OS_FS_ERR_INVALID_POINTER;
    return fs_unmount(mountpoint);
}

/*
 * Returns the number of free blocks in a file system
*/
int32 OS_fsBlocksFree(const char *name)
{

    int32 ret = chk_path(name);
    if (ret) return ret;

    return 10;
}

/*
** Returns the number of free bytes in a file system
** Note the 64 bit data type to support filesystems that
** are greater than 4 Gigabytes
*/
int32 OS_fsBytesFree(const char *name, uint64 *bytes_free)
{
    if (bytes_free == NULL) return OS_FS_ERR_INVALID_POINTER;
    int32 ret = chk_path(name);
    if (ret) return ret;

    return 10 * F_PART_DATA_SIZE;
}

/*
 *
 * Checks the health of a file system and repairs it if neccesary
*/
os_fshealth_t OS_chkfs(const char *name, boolean repair)
{
    return OS_ERR_NOT_IMPLEMENTED;
}

/*
 * Returns in the parameter the physical drive underneith the mount point
*/
int32 OS_FS_GetPhysDriveName(char * PhysDriveName, char * MountPoint)
{
    if (PhysDriveName == NULL || MountPoint == NULL) return OS_FS_ERR_INVALID_POINTER;
    int32 ret = chk_path(MountPoint);
    if (ret) return ret;
    return Filesys_GetPhysDriveName(PhysDriveName, MountPoint);
}

/*
 * This is currently not used by osal
 * Translates a OSAL Virtual file system path to a host Local path
*/
int32 OS_TranslatePath(const char *VirtualPath, char *LocalPath)
{
    if (VirtualPath == NULL || LocalPath == NULL) return OS_FS_ERR_INVALID_POINTER;
    int32 ret = chk_path(VirtualPath);
    if (ret) return ret;

    strcpy(LocalPath, VirtualPath);
    return OS_FS_SUCCESS;
}

/*
**  Returns information about the file system in an os_fsinfo_t
*/
int32 OS_GetFsInfo(os_fsinfo_t  *filesys_info)
{
    if (filesys_info == NULL) return OS_FS_ERR_INVALID_POINTER;
    return Filesys_GetFsInfo(filesys_info);
}

/******************************************************************************
** Shell API
******************************************************************************/

/* executes the shell command passed into is and writes the output of that
 * command to the file specified by the given OSAPI file descriptor */
int32 OS_ShellOutputToFile(char* Cmd, int32 OS_fd)
{
    return OS_ERR_NOT_IMPLEMENTED;
}

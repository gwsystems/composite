#include "osfilesys.h"
#include "tar.h"

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
    int32 ret = path_chk_isvalid(path);
    if (access != OS_READ_WRITE && access != OS_WRITE_ONLY) return OS_FS_ERROR;
    if (ret != OS_FS_SUCCESS) return ret;
    ret = file_create((char *) path, access);
    return ret;
}

/*
 * Open a file for reading/writing. Returns file descriptor
*/
int32 OS_open(const char *path,  int32 access,  uint32 mode)
{
    if (access != OS_READ_WRITE && access != OS_WRITE_ONLY && access != OS_READ_ONLY) {
        return OS_FS_ERROR;
    }
    int32 ret = path_chk_exists(path);
    if (ret != OS_FS_SUCCESS) return ret;
    return file_open((char *)path, access);
}

/*
 * Closes an open file.
*/
int32 OS_close(int32 filedes)
{
    if (chk_fd(filedes) != OS_FS_SUCCESS) return OS_FS_ERR_INVALID_FD;
    return file_close(filedes);
}

/*
 * Reads nbytes bytes from file into buffer
*/
int32 OS_read(int32 filedes, void *buffer, uint32 nbytes)
{
    if (chk_fd(filedes) != OS_FS_SUCCESS)  return OS_FS_ERR_INVALID_FD;
    int32 ret = file_read(filedes, buffer, nbytes);
    return ret;
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
    if (!filestats || !path) return OS_FS_ERR_INVALID_POINTER;
    int32 ret = path_chk_exists(path);
    if (ret != OS_FS_SUCCESS) return ret;
    return file_stat((char *) path, (struct hack_stat *) filestats);
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
    int32 ret = path_chk_exists(path);
    if (ret != OS_FS_SUCCESS) return ret;
    ret = file_remove((char *)path);
    return ret;
}

/*
 * Renames a file in the file system
*/
int32 OS_rename(const char *old_filename, const char *new_filename) {
    if (!old_filename || !new_filename) return OS_FS_ERR_INVALID_POINTER;
    int32 ret = path_chk_exists(old_filename);
    if (ret != OS_FS_SUCCESS) return ret;
    ret = path_chk_isvalid(new_filename);
    if (ret != OS_FS_SUCCESS) return ret;
    return file_rename((char *)old_filename, (char *)new_filename);
}

/*
 * copies a single file from src to dest
*/
int32 OS_cp(const char *src, const char *dest)
{
    if (!src || !dest) return OS_FS_ERR_INVALID_POINTER;
    char *src_path  = (char *)src;
    char *dest_path = (char *)dest;

    if (strlen(src_path) > OS_MAX_PATH_LEN) return OS_FS_ERR_PATH_TOO_LONG;
    if (strlen(dest_path) > OS_MAX_PATH_LEN) return OS_FS_ERR_PATH_TOO_LONG;
    if (strlen(path_to_name(src_path)) > OS_MAX_FILE_NAME) return OS_FS_ERR_NAME_TOO_LONG;
    if (strlen(path_to_name(dest_path)) > OS_MAX_FILE_NAME) return OS_FS_ERR_NAME_TOO_LONG;

    int32 ret = path_chk_exists(src_path);
    if (ret != OS_FS_SUCCESS) return ret;
    ret = path_chk_isvalid(dest_path);
    if (ret != OS_FS_SUCCESS) return ret;

    return file_cp(src_path, dest_path);
}

/*
 * moves a single file from src to dest
*/
int32 OS_mv(const char *src, const char *dest)
{
    char *src_path  = (char *)src;
    char *dest_path = (char *)dest;

    if (!src_path || !dest_path) return OS_FS_ERR_INVALID_POINTER;
    if (strlen(src_path) > OS_MAX_PATH_LEN) return OS_FS_ERR_PATH_TOO_LONG;
    if (strlen(dest_path) > OS_MAX_PATH_LEN) return OS_FS_ERR_PATH_TOO_LONG;
    if (strlen(path_to_name(src_path)) > OS_MAX_FILE_NAME) return OS_FS_ERR_NAME_TOO_LONG;
    if (strlen(path_to_name(dest_path)) > OS_MAX_FILE_NAME) return OS_FS_ERR_NAME_TOO_LONG;

    int32 ret = path_chk_exists(src);
    if (ret != OS_FS_SUCCESS) return ret;
    ret = path_chk_isvalid(dest);
    if (ret != OS_FS_SUCCESS) return ret;
    ret = file_mv(src_path, dest_path);

    return ret;
}

/*
 * Copies the info of an open file to the structure
*/
int32 OS_FDGetInfo(int32 filedes, OS_FDTableEntry *fd_prop)
{
    if (!fd_prop)                                   return OS_FS_ERR_INVALID_POINTER;
    if (filedes <= 0 || filedes > MAX_NUM_FILES)    return OS_FS_ERR_INVALID_FD;

    return file_FDGetInfo(filedes, fd_prop);
}

/*
** Check to see if a file is open
*/
int32 OS_FileOpenCheck(char *Filename)
{
    int32 ret = path_chk_exists(Filename);
    if (ret != OS_FS_SUCCESS) return OS_FS_ERR_INVALID_POINTER;

    struct fsobj *file = file_find(Filename);
    if (!file) return OS_INVALID_POINTER;
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
    int32 ret = path_chk_exists(Filename);
    if (ret != OS_FS_SUCCESS) return ret;
    return file_close_by_name(Filename);
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
    int32 ret = path_chk_isvalid(path);
    if (ret != OS_FS_SUCCESS) return ret;
    return file_mkdir((char *)path);
}

/*
 * Opens a directory for searching
*/
os_dirp_t OS_opendir(const char *path)
{
    if(path_chk_exists(path) != OS_FS_SUCCESS) return NULL;
    int32 FD = dir_open((char *)path);
    if (FD == 0) return NULL;
    return (os_dirp_t) FD;
}

/*
 * Closes an open directory
*/
int32 OS_closedir(os_dirp_t directory)
{
    if (!directory) return OS_FS_ERR_INVALID_POINTER;
    return dir_close((int32) directory);
}

/*
 * Rewinds an open directory
*/
void OS_rewinddir(os_dirp_t directory)
{
    if (!directory) return;
    dir_rewind((int32) directory);
    return;
}

/*
 * Reads the next object in the directory
*/
os_dirent_t *OS_readdir(os_dirp_t directory)
{
    if (!directory) return NULL;
    os_dirent_t *dir = dir_read((int32) directory);
    return dir;
}

/*
 * Removes an empty directory from the file system.
*/
int32 OS_rmdir(const char *path)
{
    int32 ret = path_chk_exists(path);
    if (ret != OS_FS_SUCCESS) return ret;
    return file_rmdir((char *)path);
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
    return fs_mount((char *)devname, mountpoint);
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
    if (!devname) return OS_FS_ERR_INVALID_POINTER;
    return rmfs(devname);
}

/*
 * Unmounts a mounted file system
*/
int32 OS_unmount(const char *mountpoint)
{
    if (!mountpoint) return OS_FS_ERR_INVALID_POINTER;
    int32 ret = path_chk_exists(mountpoint);
    if (ret != OS_FS_SUCCESS) return ret;
    return fs_unmount((char *)mountpoint);
}

/*
 * Returns the number of free blocks in a file system
*/
int32 OS_fsBlocksFree(const char *name)
{
   /*
    * I am open to discussion on how we want to handle this.
    * We could impose an artificial limit on the size of the filesystem
    * if we expect the filesystem to be used much, that is the right choice, but
    * otherwise it is not worth actually doing
    */
    return OS_ERR_NOT_IMPLEMENTED;
}

/*
** Returns the number of free bytes in a file system
** Note the 64 bit data type to support filesystems that
** are greater than 4 Gigabytes
*/
int32 OS_fsBytesFree(const char *name, uint64 *bytes_free)
{
    // see comment in fsBlocksFree
    return OS_ERR_NOT_IMPLEMENTED;
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
    if (!PhysDriveName || !MountPoint) return OS_FS_ERR_INVALID_POINTER;
    int32 ret = path_chk_exists(MountPoint);
    if (ret != OS_FS_SUCCESS) return ret;
    return filesys_GetPhysDriveName(PhysDriveName, MountPoint);
}

/*
 * This is currently not used by osal
 * Translates a OSAL Virtual file system path to a host Local path
*/
int32 OS_TranslatePath(const char *VirtualPath, char *LocalPath)
{
    return path_translate((char *) VirtualPath, LocalPath);
}

/*
**  Returns information about the file system in an os_fsinfo_t
*/
int32 OS_GetFsInfo(os_fsinfo_t  *filesys_info)
{
    if (!filesys_info) return OS_FS_ERR_INVALID_POINTER;
    return filesys_GetFsInfo(filesys_info);
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

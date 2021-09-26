#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <cos_component.h>
#include <memmgr.h>

/**
 * Basic filesystem operations are supported
 * Most of those APIs follows the normal linux filesystem APIs.
 */

word_t filesystem_fopen(const char *path, const char *flags);
word_t COS_STUB_DECL(filesystem_fopen)(const char *path, const char *flags);

int filesystem_fclose(word_t fd);
int COS_STUB_DECL(filesystem_fclose)(word_t fd);

int filesystem_ftruncate(word_t fd, unsigned long size);
int COS_STUB_DECL(filesystem_ftruncate)(word_t fd, unsigned long size);

size_t filesystem_fread(word_t fd, void *buf, size_t size);
size_t COS_STUB_DECL(filesystem_fread)(word_t fd, void *buf, size_t size);

size_t filesystem_fwrite(word_t fd, void *buf, size_t size);
size_t COS_STUB_DECL(filesystem_fwrite)(word_t fd, void *buf, size_t size);

int filesystem_fseek(word_t fd, long offset, unsigned long origin);
int COS_STUB_DECL(filesystem_fseek)(word_t fd, long offset, unsigned long origin);

unsigned long filesystem_ftell(word_t fd);
unsigned long COS_STUB_DECL(filesystem_ftell)(word_t fd);

unsigned long filesystem_fsize(word_t fd);
unsigned long COS_STUB_DECL(filesystem_fsize)(word_t fd);

/**
 * Directory isn't supported for now.
 */

/*
int fs_dir_rm(const char *path);
int COS_STUB_DECL(fs_dir_rm)(const char *path);

int fs_dir_mv(const char *path, const char *new_path);
int COS_STUB_DECL(fs_dir_mv)(const char *path, const char *new_path);

int fs_dir_mk(const char *path);
int COS_STUB_DECL(fs_dir_mk)(const char *path);

int fs_dir_open(void *dir, const char *path);
int COS_STUB_DECL(fs_dir_open)(void *dir, const char *path);

int fs_dir_close(void *dir);
int COS_STUB_DECL(fs_dir_close)(void *dir);
*/

/**
 * Functions to init share memory.
 * See blockdev's comments for more information.
 */
CCTOR int __filesystem_c_smem_init();

int __filesystem_s_smem_init(cbuf_t cid);
int COS_STUB_DECL(__filesystem_s_smem_init)(cbuf_t cid);


#endif /* FILESYSTEM_H */

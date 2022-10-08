#include <cos_component.h>
#include <llprint.h>
#include <ext4.h>
#include <ext4_mkfs.h>
#include <ps_ns.h>

extern struct ext4_blockdev *ext4_blockdev_get(void);
static struct ext4_blockdev *bd;

static struct ext4_fs        fs;
static int                   fs_type = F_SET_EXT4;
static struct ext4_mkfs_info info    = {
  .block_size = 4096,
  .journal    = true,
};
static struct ext4_bcache *bc;

struct ps_ns *ns[64];

PS_NSSLAB_CREATE(fd, sizeof(struct ext4_file), 2, 9, 6);

word_t
filesystem_fopen(const char *path, const char *flags)
{
	ps_desc_t  fd;
	ext4_file *file;
	compid_t   token;

	token = (compid_t)cos_inv_token();

	fd      = -1;
	file    = ps_nsptr_alloc_fd(ns[token], &fd);
	int ret = ext4_fopen(file, path, flags);
	//printc("ret %d\n", ret);

	//printc("mp %p\n", file->mp);

	return (word_t)fd;
}

int
filesystem_fclose(word_t fd)
{
	compid_t token;
	token = (compid_t)cos_inv_token();

	return ext4_fclose(ps_nsptr_lkup_fd(ns[token], fd));
}

int
filesystem_ftruncate(word_t fd, unsigned long size)
{
	compid_t token;
	token = (compid_t)cos_inv_token();

	return ext4_ftruncate(ps_nsptr_lkup_fd(ns[token], fd), size);
}

size_t
filesystem_fread(word_t fd, void *buf, size_t size)
{
	size_t   rcnt;
	compid_t token;
	token = (compid_t)cos_inv_token();

	int ret = ext4_fread(ps_nsptr_lkup_fd(ns[token], fd), buf, size, &rcnt);
	//printc("fread ret %d rcnt %d\n", ret, rcnt);

	return rcnt;
}

size_t
filesystem_fwrite(word_t fd, void *buf, size_t size)
{
	size_t   wcnt;
	compid_t token;

	token = (compid_t)cos_inv_token();

	//printc("fwrite writing %s\n", buf);

	int ret = ext4_fwrite(ps_nsptr_lkup_fd(ns[token], fd), buf, size, &wcnt);
	//printc("fwrite ret %d wcnt %d\n", ret, wcnt);

	return wcnt;
}

int
filesystem_fseek(word_t fd, long offset, unsigned long origin)
{
	compid_t token;
	token = (compid_t)cos_inv_token();

	return ext4_fseek(ps_nsptr_lkup_fd(ns[token], fd), offset, origin);
}

unsigned long
filesystem_ftell(word_t fd)
{
	compid_t token;
	token = (compid_t)cos_inv_token();

	return ext4_ftell(ps_nsptr_lkup_fd(ns[token], fd));
}

unsigned long
filesystem_fsize(word_t fd)
{
	compid_t token;
	token = (compid_t)cos_inv_token();

	return ext4_fsize(ps_nsptr_lkup_fd(ns[token], fd));
}

void
cos_init(void)
{
	for (int i = 0; i < 64; i++) { ns[i] = ps_nsptr_create_slab_fd(); }

	bd = ext4_blockdev_get();
	// ext4_dmask_set(DEBUG_ALL);

	ext4_mkfs(&fs, bd, &info, fs_type);
	ext4_device_register(bd, "ext4_fs");
	ext4_mount("ext4_fs", "/", false);
}

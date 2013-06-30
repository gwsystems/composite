#include <cos_component.h>
#include <print.h>
#include <stdio.h>
#include <torrent.h>
#include <evt.h>
#include <sys/stat.h>

#include "../../../lib/libccv/ccv.h"
#include "../../../lib/dietlibc-0.29/include/cos_syscalls.h"

// torrent related
int cos_open(const char *pathname, int flags, int mode);        // FLAGS not implemented, however bbfedtect only use O_RDONLY 
int cos_close(int fd);                                          // no error ret
ssize_t cos_read(int fd, void *buf, size_t count);              // done
ssize_t cos_write(int fd, const void *buf, size_t count);       // done
off_t cos_lseek(int fd, off_t offset, int whence);              // SEEK_END flag is not implemented, logic should be reworked
//int cos_fstat(int fd, struct stat *buf);                        // (not) done should alwas be success(0)

// cos_alloc related
void *cos_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int cos_munmap(void *start, size_t length);
void *cos_mremap(void *old_address, size_t old_size, size_t new_size, int flags);

// default
int default_syscall(void);

// test program
void face_detect(void);

char buffer[1024]; // must put this outside the cos_init, or buffer will have a wrong address

void
cos_init(void *args)
{
        libc_syscall_override(cos_open, __NR_open);
        libc_syscall_override(cos_close, __NR_close);
        libc_syscall_override(cos_read, __NR_read);
        libc_syscall_override(cos_write, __NR_write);
        libc_syscall_override(cos_mmap, __NR_mmap);
        libc_syscall_override(cos_munmap, __NR_munmap);
        libc_syscall_override(cos_mremap, __NR_mremap);
        libc_syscall_override(cos_lseek, __NR_lseek);
//      libc_syscall_override(cos_fstat, __NR_fstat);

        face_detect();

        printc("tests finished\n");

        return;
}

void
face_detect(void)
{
        int i;
        char *argv[3];

        argv[0] = NULL; // take the position
        argv[1] = "photo.bmp"; // the picture file (.BMP) to be detected
        argv[2] = "face"; // the face data

        ccv_enable_default_cache();
        ccv_dense_matrix_t* image = 0;
        ccv_bbf_classifier_cascade_t* cascade = ccv_load_bbf_classifier_cascade(argv[2]);
        ccv_read(argv[1], &image, CCV_IO_GRAY | CCV_IO_ANY_FILE);
        ccv_array_t* seq = ccv_bbf_detect_objects(image, &cascade, 1, ccv_bbf_default_params);
        for (i = 0; i < seq->rnum; i++)
        {
                ccv_comp_t* comp = (ccv_comp_t*)ccv_array_get(seq, i);
                printc("%d %d %d %d\n", comp->rect.x, comp->rect.y, comp->rect.width, comp->rect.height);
        }
        printc("total : %d\n", seq->rnum);
        ccv_array_free(seq);
        ccv_matrix_free(image);
        ccv_bbf_classifier_cascade_free(cascade);
        ccv_disable_cache();

        return;
}

int
cos_open(const char *pathname, int flags, int mode)
{
        // mode param is only for O_CREAT in flags
        printc("syscall : open(%s, %d, %o)\n", pathname, flags, mode);

        td_t t;
        long evt;
        evt = evt_split(cos_spd_id(), 0, 0);
        assert(evt > 0);
        t = tsplit(cos_spd_id(), td_root, pathname, strlen(pathname), TOR_ALL, evt);

        if (t <= 0) {
                printc("open() failed!\n");
                assert(0);
        }

        return t;
}

int
cos_close(int fd)
{
        printc("syscall: close(%d)\n", fd);

        trelease(cos_spd_id(), fd); // return void, use tor_lookup?

        return 0; // return -1 if failed
}

ssize_t
cos_read(int fd, void *buf, size_t count)
{
        printc("syscall: read(%d, %p, %d)\n", fd, buf, count);

        int ret = tread_pack(cos_spd_id(), fd, buf, count);

        //assert(ret < 0);

        return ret;
}

ssize_t
cos_write(int fd, const void *buf, size_t count)
{
        printc("syscall: write(%d, %p, %d)\n", fd, buf, count);

        int ret = twrite_pack(cos_spd_id(), fd, buf, count);

        //assert(ret < 0);

        return ret;
}

void *
cos_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
        if (addr != NULL) {
                printc("parameter void *addr is not supported!\n");
                assert(0);
        }

        if (fd != -1) {
                printc("file mapping is not supported!\n");
                assert(0);
        }

        printc("syscall: old_mmap(%p, %u, %d, %d, %d, %ld)\n", addr, length, prot, flags, fd, offset);

        void *ret = do_mmap(length);

        if (ret == (void *)-1) { // return value comes from man page
                printc("mmap() failed!\n");
                assert(0);
        }

        return ret;
}

int
cos_munmap(void *start, size_t length)
{
        printc("syscall: munmap(%p, %u)", start, length);

        int ret = do_munmap(start, length);
        assert(ret == -1);

        return ret;
}

void *
cos_mremap(void *old_address, size_t old_size, size_t new_size, int flags)
{
        printc("syscall: mremap(%p, %d, %d, %d)\n", old_address, old_size, new_size, flags);

        do_munmap(old_address, old_size);

        return do_mmap(new_size);
}

off_t
cos_lseek(int fd, off_t offset, int whence)
{
        printc("syscall: lseek(%d, %ld, %d)\n", fd, offset, whence);
        // TODO: we can use a simpler twmeta_pack(td_t td, const char *key, const char *val)
        char val[8]; // TODO: length number need to be selected
        int ret = -1;

        if (whence == SEEK_SET) {
                snprintf(val, 8, "%ld", offset);
                printc("set offset to %s\n", val);
                ret = twmeta(cos_spd_id(), fd, "offset", strlen("offset"), val, strlen(val));
                assert(ret == 0);
        } else if (whence == SEEK_CUR) {
                // return value not checked
                char offset_curr[8];
                trmeta(cos_spd_id(), fd, "offset", strlen("offset"), offset_curr, 8);
                printc("curr offset is %s\n", offset_curr);
                snprintf(val, 8, "%ld", atol(offset_curr) + offset);
                printc("set offset to %s\n", val);
                ret = twmeta(cos_spd_id(), fd, "offset", strlen("offset"), val, strlen(val));
                assert(ret == 0);
        } else if (whence == SEEK_END) {
                printc("lseek::SEEK_END not implemented !\n");
                assert(0);
                // TODO: how to get the length of the file?
        }

        if (ret != -1) return atoi(val);
        else return ret;
}

/*
int
cos_fstat(int fd, struct stat *buf)
{
        printf("syscall: fstat\n");
        
        return 0;
}
*/
int
default_syscall()
{
        int syscall_num;
        asm volatile("movl %%eax,%0" : "=r" (syscall_num));
        printc("WARNING: Component %ld calling undifined system call %d\n", cos_spd_id(), syscall_num);
        assert(0);

        return 0;
}

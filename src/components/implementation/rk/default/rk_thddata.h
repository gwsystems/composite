#ifndef RK_THDDATA_H
#define RK_THDDATA_H

#include <cos_kernel_api.h>
#include <cos_types.h>
#include <sys/socket.h>
#include <rk.h>

#define RK_THD_MAX_RES_TYPE 4

struct rk_thddata {
	cbuf_t shmid;
	vaddr_t shmaddr;

	int sockfds[RK_THD_MAX_RES_TYPE];
} rk_thdinfo[MAX_NUM_THREADS];

int rump___sysimpl_close(int);

static void
rk_thddata_init(void)
{
	int i;

	memset(rk_thdinfo, 0, sizeof(struct rk_thddata) * MAX_NUM_THREADS);
	for (i = 0; i < MAX_NUM_THREADS; i++) {
		int j;
		struct rk_thddata *ti = &rk_thdinfo[i];

		for (j = 0; j < RK_THD_MAX_RES_TYPE; j++) {
			ti->sockfds[j] = -1;
		}
	}

}

static inline vaddr_t
rk_thddata_shm_get(cbuf_t *id)
{
	struct rk_thddata *ti = &rk_thdinfo[cos_thdid()];

	if (id == NULL) return 0;

	if (*id == 0) {
		if (ti->shmid == 0) {
			ti->shmid = memmgr_shared_page_allocn(RK_MAX_PAGES, &ti->shmaddr);
			assert(ti->shmid && ti->shmaddr);
		}
		*id = ti->shmid;
	} else {
		unsigned long npages = 0;

		if (*id == ti->shmid) goto done;

		if (ti->shmid && ti->shmid != *id) {
			int i;

			/*
			 * FIXME: Not sure if this is the best way.
			 * Right now, if the shared memory id is different from the
			 * previous set of calls, assuming this is coming because the
			 * client has been rebooted!
			 *
			 * Plus, only tracking socket identifiers or descriptors and
			 * closing them up on that reboot!
			 */

			PRINTLOG(PRINT_WARN, "Rebooted? Cleaning up any stale sockets!\n");
			for (i = 0; i < RK_THD_MAX_RES_TYPE; i++) {
				if (ti->sockfds[i] == -1) continue;

				rump___sysimpl_close(ti->sockfds[i]);
				ti->sockfds[i] = -1;
			}
		}
		npages = memmgr_shared_page_map(*id, &ti->shmaddr);
		assert(npages == RK_MAX_PAGES && ti->shmaddr);
		ti->shmid = *id;
	}

done:
	assert(ti->shmaddr);

	return ti->shmaddr;
}

static inline int
rk_thddata_sock_set(int sockfd)
{
	struct rk_thddata *ti = &rk_thdinfo[cos_thdid()];
	int i;

	assert(sockfd >= 0);
	for (i = 0; i < RK_THD_MAX_RES_TYPE; i++) {
		if (ti->sockfds[i] >= 0) continue;

		ti->sockfds[i] = sockfd;

		return i;
	}

	return -1;
}

#endif /* RK_THDDATA_H */

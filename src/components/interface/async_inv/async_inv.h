#ifndef   	ASYNC_INV_H
#define   	ASYNC_INV_H

#include <content_req.h>

long content_create(spdid_t spdid, long evt_id, struct cos_array *data);
long content_split(spdid_t spdid, long conn_id, long evt_id);
int content_remove(spdid_t spdid, long conn_id);
int content_write(spdid_t spdid, long conn_id, char *data, int sz);
int content_read(spdid_t spdid, long conn_id, char *data, int sz);
content_req_t async_open(spdid_t spdid, long evt_id, struct cos_array *data);
int async_close(spdid_t spdid, content_req_t cr);
int async_request(spdid_t spdid, content_req_t cr, struct cos_array *data);
int async_retrieve(spdid_t spdid, content_req_t cr, struct cos_array *data, int *more);


#endif 	    /* !ASYNC_INV_H */

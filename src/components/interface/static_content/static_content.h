#ifndef   	STATIC_CONTENT_H
#define   	STATIC_CONTENT_H

#include <content_req.h>

content_req_t static_open(spdid_t spdid, long evt_id, struct cos_array *data);
int static_request(spdid_t spdid, content_req_t cr, struct cos_array *data);
int static_retrieve(spdid_t spdid, content_req_t cr, struct cos_array *data, int *more);
int static_close(spdid_t spdid, content_req_t cr);

#endif 	    /* !STATIC_CONTENT_H */

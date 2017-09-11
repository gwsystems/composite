#ifndef   	CONTENT_MUX_H
#define   	CONTENT_MUX_H

#include <content_req.h>

content_req_t content_open(spdid_t spdid, long evt_id, struct cos_array *data);
int content_request(spdid_t spdid, content_req_t cr, struct cos_array *data);
int content_retrieve(spdid_t spdid, content_req_t cr, struct cos_array *data, int *more);
int content_close(spdid_t spdid, content_req_t cr);

#endif 	    /* !CONTENT_MUX_H */

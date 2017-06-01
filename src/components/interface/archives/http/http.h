#ifndef   	HTTP_H
#define   	HTTP_H

long content_split(spdid_t spdid, long conn_id, long evt_id);
int content_write(spdid_t spdid, long connection_id, char *reqs, int sz);
int content_read(spdid_t spdid, long connection_id, char *buff, int sz);
long content_create(spdid_t spdid, long evt_id, struct cos_array *d);
int content_remove(spdid_t spdid, long conn_id);

#endif 	    /* !HTTP_H */

#ifndef   	NET_PORTNS_H
#define   	NET_PORTNS_H

int portmgr_new(spdid_t spdid);
int portmgr_bind(spdid_t spdid, u16_t port);
void portmgr_free(spdid_t spdid, u16_t port);

#endif 	    /* !NET_PORTNS_H */

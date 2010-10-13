#ifndef   	NET_IF_H
#define   	NET_IF_H

int netif_event_create(spdid_t spdid);
int netif_event_release(spdid_t spdid);
int netif_event_wait(spdid_t spdid, struct cos_array *d);
int netif_event_xmit(spdid_t spdid, struct cos_array *d);

unsigned long netif_upcall_cyc(void);

#endif 	    /* !NET_IF_H */

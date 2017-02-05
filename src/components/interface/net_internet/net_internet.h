#ifndef   	NET_INTERNET_H
#define   	NET_INTERNET_H

int ip_netif_create(spdid_t spdid);
int ip_netif_release(spdid_t spdid);
int ip_wait(spdid_t spdid, struct cos_array *d);
int ip_xmit(spdid_t spdid, struct cos_array *d);

#endif 	    /* !NET_INTERNET_H */

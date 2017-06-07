#ifndef FAILURE_NOTIF_H
#define FAILURE_NOTIF_H

void failure_notif_fail(spdid_t caller, spdid_t failed);
/* wait till the failure is recovered from... */
void failure_notif_wait(spdid_t caller, spdid_t failed);

#endif 	    /* FAILURE_NOTIF_H */

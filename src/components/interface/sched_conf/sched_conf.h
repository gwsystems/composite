#ifndef   	SCHED_CONF_H
#define   	SCHED_CONF_H

int sched_comp_config_default(spdid_t spdid, spdid_t target, struct cos_array *data);
spdid_t sched_comp_config(spdid_t spdid, int index, struct cos_array *data);
int sched_comp_config_initstr(spdid_t spdid, struct cos_array *data);

#endif 	    /* !SCHED_CONF_H */

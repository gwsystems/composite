#ifndef   	SCHED_CONFIG_H
#define   	SCHED_CONFIG_H

struct comp_sched_info {
	spdid_t spdid;
	char *name, *sched_str;
};

#define MAX_COMP_IDX 27
static struct comp_sched_info comp_info[MAX_COMP_IDX+2] = 
{ 
	{ .spdid = 0, .name = "c0.o", .sched_str = "" }, 
	{ .spdid = 1, .name = "fprr.o", .sched_str = "" }, 
	{ .spdid = 2, .name = "mpd.o", .sched_str = "a4" }, 
	{ .spdid = 3, .name = "l.o", .sched_str = "a8" }, 
	{ .spdid = 4, .name = "mm.o", .sched_str = "" }, 
	{ .spdid = 5, .name = "print.o", .sched_str = "" }, 
	{ .spdid = 6, .name = "te.o", .sched_str = "a3" }, 
	{ .spdid = 7, .name = "net.o", .sched_str = "a6" }, 
	{ .spdid = 8, .name = "e.o", .sched_str = "a3" }, 
	{ .spdid = 9, .name = "fd.o", .sched_str = "a8" }, 
	{ .spdid = 10, .name = "conn.o", .sched_str = "a9" }, 
	{ .spdid = 11, .name = "http.o", .sched_str = "a8" }, 
	{ .spdid = 12, .name = "stat.o", .sched_str = "a25" }, 
	{ .spdid = 13, .name = "st.o", .sched_str = "" }, 
	{ .spdid = 14, .name = "cm.o", .sched_str = "a7" }, 
	{ .spdid = 15, .name = "sc.o", .sched_str = "a6" }, 
	{ .spdid = 16, .name = "if.o", .sched_str = "a5" }, 
	{ .spdid = 17, .name = "ip.o", .sched_str = "" }, 
	{ .spdid = 18, .name = "ainv.o", .sched_str = "a6" }, 
	{ .spdid = 19, .name = "fn.o", .sched_str = "" }, 
	{ .spdid = 20, .name = "fd2.o", .sched_str = "a8" }, 
	{ .spdid = 21, .name = "cgi.o", .sched_str = "a9" }, 
	{ .spdid = 22, .name = "fd3.o", .sched_str = "a8" }, 
	{ .spdid = 23, .name = "cgi2.o", .sched_str = "a9" }, 
	{ .spdid = 24, .name = "port.o", .sched_str = "" }, 
	{ .spdid = 25, .name = "ainv2.o", .sched_str = "a6" }, 
	{ .spdid = 26, .name = "schedconf.o", .sched_str = "" }, 
	{ .spdid = 27, .name = "bc.o", .sched_str = "" }, 
	{ .spdid = -1, .name = NULL, .sched_str = NULL }
};

static char *spd_name_map_name(spdid_t spdid)
{
	struct comp_sched_info *csi;

	if (spdid > MAX_COMP_IDX+1) return NULL;
	csi = &comp_info[spdid];
	return csi->name;
}

static spdid_t spd_name_map_id(char *name)
{
	int i;

	if (!name) return 0;
	for (i = 0 ; i < MAX_COMP_IDX+1 ; i++) {
		if (strstr(name, comp_info[i].name)) return comp_info[i].spdid;
	}
	return 0;
}

#endif 	    /* !SCHED_CONFIG_H */

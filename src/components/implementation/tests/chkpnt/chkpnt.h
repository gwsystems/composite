#ifndef CHKPNT
#define CHKPNT

struct chkpnt {
    struct crt_comp *c;
    struct crt_comp_resources *res;
    /* some kind of pointer to the memory */

};

int chkpnt_create(struct chkpnt *cp, struct crt_comp *c, struct crt_comp_rescource *r);
int chkpnt_restore(struct crt_comp *c, struct chkpnt *cp);
/* 
 * new_cp is the space for the new chkpnt, 
 * replica is the chkpnt we're modeling new_cp after, 
 * c is the dead component to be recyled 
 */
int chkpnt_recycle(struct chkpnt *new_cp, struct chkpnt *replica, struct crt_comp *c);

#endif //CHKPNT




#ifndef COS_LOCK_H
#define COS_LOCK_H

int __attribute__((format(printf,1,2))) printc(char *fmt, ...);

void intr_disable(void);
int  intr_getdisabled(int intr);
void intr_enable(void);
void intr_delay(int isrthd);
void intr_pending(int pending, int tid, int rcving);


#endif

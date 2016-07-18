#ifndef COS_LOCK_H
#define COS_LOCK_H

int __attribute__((format(printf,1,2))) printc(char *fmt, ...);

inline void intr_disable(void);
inline int  intr_getdisabled(int intr);
inline void intr_enable(void);
inline void intr_delay(int isrthd);
inline void intr_pending(int pending, int tid, int rcving);


#endif

#ifndef CHAN_CRT_H
#define CHAN_CRT_H

#define CHAN_CRT_NSLOTS 4
#define CHAN_CRT_ITEM_TYPE unsigned long
#define CHAN_CRT_ITEM_SZ sizeof(CHAN_CRT_ITEM_TYPE)

int           chan_out(unsigned long item);
unsigned long chan_in(void);

#endif /* CHAN_CRT_H */

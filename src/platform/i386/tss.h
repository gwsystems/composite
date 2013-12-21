/* Based on code from Pintos. See LICENSE.pintos for licensing information */

#ifndef USERPROG_TSS_H
#define USERPROG_TSS_H

#include "types.h"

struct tss;
void tss__init (void);
struct tss *tss_get (void);
void tss_update (void);

#endif

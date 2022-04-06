#ifndef PONGSHVAS_H
#define PONGSHVAS_H

#include <cos_component.h>

unsigned long  pongshvas_send(void);
void           pongshvas_rcv_and_update(unsigned long *shared);

#endif /* PONGSHVAS_H */

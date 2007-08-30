/* 
 * Author: Gabriel Parmer
 * License: GPLv2
 */

#ifndef IPC_H
#define IPC_H

#include <thread.h>
#include <spd.h>

//int ipc_invoke_spd(int capability, ...);
void ipc_init(void);
//void ipc_set_current_spd(struct spd *spd);

#endif

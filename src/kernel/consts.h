#pragma once

/*
 * Kernel implementation constants
 */

#define COS_INVSTK_SIZE 2

#define THD_STATE_EVT_AWAITING   1   /* after calling await_asnd */
#define THD_STATE_EVT_TRIGGERED  2   /* after being triggered by trigger_asnd */
#define THD_STATE_EXECUTING      3   /* normal state, executable */
#define THD_STATE_IPC_DEPENDENCY 4   /* IPC dependency on another thread */
#define THD_STATE_IPC_AWAIT      5   /* awaiting IPC from another thread */

/**
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Copyright The George Washington University, Gabriel Parmer,
 * gparmer@gwu.edu, 2012
 */

/*
 * The Composite Hardware Abstraction Layer, or Hijack Abstraction
 * Layer (cHAL) is the layer that defines the platform-specific
 * functionality that requires specific implementations not only for
 * different architectures (e.g. x86-32 vs. -64), but also when
 * booting from the bare-metal versus using the Hijack techniques.
 * This file documents the functions that must be implemented within
 * the platform code, and how they interact with the _Composite_
 * kernel proper.
 */

#pragma once

#include <cos_types.h>

/*
 * Namespacing in the cHAL: chal_<family>_<operation>(...).  <family>
 * is the family of operations such as pgtbl or addr operations, and
 * <operation> is the operation to perform on that family of
 * manipulations.
 */

/* IPI sending */
void chal_send_ipi(coreid_t cpu_id);

/* Timer programming */
void chal_idle(void);
void chal_timer_program(cos_time_t cycles);

/* Initialization of all hardware */
void chal_init(void);

extern void printk(const char *fmt, ...);
void        chal_khalt(void);

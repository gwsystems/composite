/**
 * Copyright 2016 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef HW_H
#define HW_H

#include "component.h"
#include "thd.h"
#include "chal/call_convention.h"
#include "inv.h"

#define HW_IRQ_TOTAL 256
#define HW_IRQ_EXTERNAL_MIN 32
#define HW_IRQ_EXTERNAL_MAX 63

struct cap_asnd hw_asnd_caps[NUM_CPU][HW_IRQ_TOTAL];

struct cap_hw {
	struct cap_header h;
	u32_t             hw_bitmap;
} __attribute__((packed));

static void
hw_asndcap_init(void)
{
	memset(&hw_asnd_caps, 0, sizeof(struct cap_asnd) * HW_IRQ_TOTAL * NUM_CPU);
}

/*
 * FIXME: This is broken as it allows someone to create a hwcap with an
 * arbitrary bitmap.  This should be changed to only create a hwcap
 * from another, and only with a subset of the bitmap.  Any other HW
 * resources should not be passed on.
 */
static int
hw_activate(struct captbl *t, capid_t cap, capid_t capin, u32_t bitmap)
{
	struct cap_hw *hwc;
	int            ret;

	hwc = (struct cap_hw *)__cap_capactivate_pre(t, cap, capin, CAP_HW, &ret);
	if (!hwc) return ret;

	hwc->hw_bitmap = bitmap;

	__cap_capactivate_post(&hwc->h, CAP_HW);

	return 0;
}

static int
hw_deactivate(struct cap_captbl *t, capid_t capin, livenessid_t lid)
{
	return cap_capdeactivate(t, capin, CAP_HW, lid);
}

static int
hw_attach_rcvcap(struct cap_hw *hwc, hwid_t hwid, struct cap_arcv *rcvc, capid_t rcv_cap)
{
	if (hwid < HW_IRQ_EXTERNAL_MIN || hwid > HW_IRQ_EXTERNAL_MAX) return -EINVAL;
	if (!(hwc->hw_bitmap & (1 << (hwid - HW_IRQ_EXTERNAL_MIN)))) return -EINVAL;
	if (hw_asnd_caps[get_cpuid()][hwid].h.type == CAP_ASND) return -EEXIST;

	return asnd_construct(&hw_asnd_caps[get_cpuid()][hwid], rcvc, rcv_cap, 0, 0);
}

static int
hw_detach_rcvcap(struct cap_hw *hwc, hwid_t hwid)
{
	if (hwid < HW_IRQ_EXTERNAL_MIN || hwid > HW_IRQ_EXTERNAL_MAX) return -EINVAL;
	if (!(hwc->hw_bitmap & (1 << (hwid - HW_IRQ_EXTERNAL_MIN)))) return -EINVAL;

	/*
	 * FIXME: Need to synchronize using __xx_pre and
	 *        __xx_post perhaps in asnd_deconstruct()
	 */
	memset(&hw_asnd_caps[get_cpuid()][hwid], 0, sizeof(struct cap_asnd));

	return 0;
}

static inline int
hw_introspect(struct cap_hw *hwc, unsigned long op, unsigned long a, unsigned long *retval)
{
        switch(op) {
        case HW_CACHE_FLUSH: chal_flush_cache(); *retval = 0; break;
	case HW_CORE_IPI_SND_GET: *retval = chal_core_ipi_snd_get(a); break;
	case HW_CORE_IPI_RCV_GET: *retval = chal_core_ipi_rcv_get(a); break;
        default: return -EINVAL;
        }
        return 0;
}

#endif /* HW_H */

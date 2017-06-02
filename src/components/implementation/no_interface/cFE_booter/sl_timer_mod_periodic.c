#include <sl.h>
#include <sl_consts.h>
#include <sl_plugins.h>

void
sl_timeout_mod_expended(microsec_t now, microsec_t oldtimeout)
{
	cycles_t offset;

	assert(now >= oldtimeout);

	/* in virtual environments, or with very small periods, we might miss more than one period */
	offset = (now - oldtimeout) % sl_timeout_period_get();
	sl_timeout_oneshot(now + sl_timeout_period_get() - offset);
}

void
sl_timeout_mod_init(void)
{
	sl_timeout_period(SL_PERIOD_US);
}

#include <sl.h>
#include <sl_consts.h>
#include <sl_plugins.h>

void
sl_timeout_mod_expended(microsec_t now, microsec_t oldtimeout)
{
	assert(now >= oldtimeout);

	/* in virtual environments, or with very small periods, we might miss more than one period */
	sl_timeout_oneshot(now + (now-oldtimeout) % sl_timeout_period_get());
}

void
sl_timeout_mod_init(void)
{
	sl_timeout_period(SL_PERIOD_US);
}

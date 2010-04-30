#include <cos_component.h>
#include <print.h>
#include <timed_blk.h>
#include <hlc.h>

void call_low()
{
    printc("In low\n");
    timed_event_block(cos_spd_id(), 100);
}

void call_high()
{
    printc("In high\n");
    
}

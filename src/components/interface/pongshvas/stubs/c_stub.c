#include <cos_component.h>
#include <cos_stubs.h>


COS_CLIENT_STUB(unsigned long *, pongshvas_send)
{
    COS_CLIENT_INVCAP_SHARED;

    unsigned long *(*callgate)() = (unsigned long *(*)(void)) uc->callgate_addr;
    //printc("PTR %p\n", callgate);
    return callgate();
}
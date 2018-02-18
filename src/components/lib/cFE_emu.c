#include <llprint.h>

#include <cfe_error.h>
#include <cfe_evs.h>

// FIXME: Query the cFE to decide whether printf is enabled
int is_printf_enabled = 1;

void OS_printf(const char *string, ...)
{
    if(is_printf_enabled) {
        char s[OS_BUFFER_SIZE];
        va_list arg_ptr;
        int ret, len = OS_BUFFER_SIZE;

        va_start(arg_ptr, string);
        ret = vsnprintf(s, len, string, arg_ptr);
        va_end(arg_ptr);
        cos_llprint(s, ret);
    }
}


// TODO: Actually emulate these calls
int32 CFE_EVS_Register (void                 *Filters,           /* Pointer to an array of filters */
                        uint16               NumFilteredEvents,  /* How many elements in the array? */
                        uint16               FilterScheme)      /* Filtering Algorithm to be implemented */
{
    printc("CFE_EVS_Register called...\n");
    return CFE_SUCCESS;
}

int32  CFE_SB_CreatePipe(CFE_SB_PipeId_t *PipeIdPtr,
                         uint16  Depth,
                         const char *PipeName)
{
    printc("CFE_SB_CreatePipe called...\n");
    return CFE_SUCCESS;
}

void CFE_SB_InitMsg(void           *MsgPtr,
                    CFE_SB_MsgId_t MsgId,
                    uint16         Length,
                    boolean        Clear )
{
    printc("CFE_SB_InitMsg called...\n");
}

int32 CFE_EVS_SendEvent (uint16 EventID,
                         uint16 EventType,
                         const char *Spec, ... )
{
    printc("CFE_EVS_SendEvent called...\n");
    return CFE_SUCCESS;
}

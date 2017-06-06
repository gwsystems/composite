#include <string.h>

#include "cFE_util.h"

#include "sl.h"
#include "sl_consts.h"

#include "gen/osapi.h"
#include "gen/cfe_psp.h"

/*
** Function prototypes
*/

/*
** PSP entry point and reset routines
*/
void CFE_PSP_Main(uint32 ModeId, char *StartupFilePath)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
}

/*
** CFE_PSP_Main is the entry point that the real time OS calls to start our
** software. This routine will do any BSP/OS specific setup, then call the
** entrypoint of the flight software ( i.e. the cFE main entry point ).
** The flight software (i.e. cFE ) should not call this routine.
*/

void CFE_PSP_GetTime(OS_time_t *LocalTime)
{
    OS_GetLocalTime(LocalTime);
}
/* This call gets the local time from the hardware on the Vxworks system
 * on the mcp750s
 * on the other os/hardware setup, it will get the time the normal way */


void CFE_PSP_Restart(uint32 resetType)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
}
/*
** CFE_PSP_Restart is the entry point back to the BSP to restart the processor.
** The flight software calls this routine to restart the processor.
*/


uint32 CFE_PSP_GetRestartType(uint32 *restartSubType )
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}
/*
** CFE_PSP_GetRestartType returns the last reset type and if a pointer to a valid
** memory space is passed in, it returns the reset sub-type in that memory.
** Right now the reset types are application specific. For the cFE they
** are defined in the cfe_es.h file.
*/


void CFE_PSP_FlushCaches(uint32 type, cpuaddr address, uint32 size);
/*
** This is a BSP specific cache flush routine
*/

uint32 CFE_PSP_GetProcessorId(void)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}
/*
** CFE_PSP_GetProcessorId returns the CPU ID as defined by the specific board
** and BSP.
*/


uint32 CFE_PSP_GetSpacecraftId(void)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}
/*
** CFE_PSP_GetSpacecraftId retuns the Spacecraft ID (if any )
*/


uint32 CFE_PSP_Get_Timer_Tick(void)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}
/*
** CFE_PSP_Get_Timer_Tick returns the underlying OS timer tick value
** It is used for the performance monitoring software
*/

uint32 CFE_PSP_GetTimerTicksPerSecond(void)
{
    // (usec / sec) / (usec / tick)
    return 1000000 / SL_PERIOD_US;
}

/*
** CFE_PSP_GetTimerTicksPerSecond provides the resolution of the least significant
** 32 bits of the 64 bit time stamp returned by CFE_PSP_Get_Timebase in timer
** ticks per second.  The timer resolution for accuracy should not be any slower
** than 1000000 ticks per second or 1 us per tick
*/

uint32 CFE_PSP_GetTimerLow32Rollover(void)
{
    // This is a pessimistic assumption
    // TODO: Figure out if we can be more optimistic
    return 1000000;
}
/*
** CFE_PSP_GetTimerLow32Rollover provides the number that the least significant
** 32 bits of the 64 bit time stamp returned by CFE_PSP_Get_Timebase rolls over.
** If the lower 32 bits rolls at 1 second, then the CFE_PSP_TIMER_LOW32_ROLLOVER
** will be 1000000.  if the lower 32 bits rolls at its maximum value (2^32) then
** CFE_PSP_TIMER_LOW32_ROLLOVER will be 0.
*/

void CFE_PSP_Get_Timebase(uint32 *Tbu, uint32 *Tbl)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
}
/*
** CFE_PSP_Get_Timebase
*/

uint32 CFE_PSP_Get_Dec(void)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}
/*
** CFE_PSP_Get_Dec
*/


int32 CFE_PSP_InitProcessorReservedMemory(uint32 RestartType)
{
    // Since all operations on reserved memory are unimplemented, it's safe to
    // do nothing here
    return 0;
}
/*
** CFE_PSP_InitProcessorReservedMemory initializes all of the memory in the
** BSP that is preserved on a processor reset. The memory includes the
** Critical Data Store, the ES Reset Area, the Volatile Disk Memory, and
** the User Reserved Memory. In general, the memory areas will be initialized
** ( cleared ) on a Power On reset, and preserved during a processor reset.
*/

int32 CFE_PSP_GetCDSSize(uint32 *SizeOfCDS)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}
/*
** CFE_PSP_GetCDSSize fetches the size of the OS Critical Data Store area.
*/

int32 CFE_PSP_WriteToCDS(void *PtrToDataToWrite, uint32 CDSOffset, uint32 NumBytes)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}
/*
** CFE_PSP_WriteToCDS writes to the CDS Block.
*/

int32 CFE_PSP_ReadFromCDS(void *PtrToDataToRead, uint32 CDSOffset, uint32 NumBytes)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}
/*
** CFE_PSP_ReadFromCDS reads from the CDS Block
*/

#define RESET_AREA_SIZE 132000
char RESET_AREA[RESET_AREA_SIZE];

int32 CFE_PSP_GetResetArea (cpuaddr *PtrToResetArea, uint32 *SizeOfResetArea)
{
    *PtrToResetArea = (cpuaddr) RESET_AREA;
    *SizeOfResetArea = RESET_AREA_SIZE;
    return OS_SUCCESS;
}
/*
** CFE_PSP_GetResetArea returns the location and size of the ES Reset information area.
** This area is preserved during a processor reset and is used to store the
** ER Log, System Log and reset related variables
*/

int32 CFE_PSP_GetUserReservedArea(cpuaddr *PtrToUserArea, uint32 *SizeOfUserArea )
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}
/*
** CFE_PSP_GetUserReservedArea returns the location and size of the memory used for the cFE
** User reserved area.
*/


int32 CFE_PSP_GetVolatileDiskMem(cpuaddr *PtrToVolDisk, uint32 *SizeOfVolDisk )
{
   int32 return_code;

   if ( SizeOfVolDisk == NULL )
   {
      return_code = CFE_PSP_ERROR;
   }
   else
   {
      *PtrToVolDisk = 0;
      *SizeOfVolDisk = 0;
      return_code = CFE_PSP_SUCCESS;
   }

   return(return_code);

}
/*
** CFE_PSP_GetVolatileDiskMem returns the location and size of the memory used for the cFE
** volatile disk.
*/

int32 CFE_PSP_GetKernelTextSegmentInfo(cpuaddr *PtrToKernelSegment, uint32 *SizeOfKernelSegment)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}
/*
** CFE_PSP_GetKernelTextSegmentInfo returns the location and size of the kernel memory.
*/

int32 CFE_PSP_GetCFETextSegmentInfo(cpuaddr *PtrToCFESegment, uint32 *SizeOfCFESegment)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}
/*
** CFE_PSP_GetCFETextSegmentInfo returns the location and size of the kernel memory.
*/

void CFE_PSP_WatchdogInit(void)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
}
/*
** CFE_PSP_WatchdogInit configures the watchdog timer.
*/

void CFE_PSP_WatchdogEnable(void)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
}
/*
** CFE_PSP_WatchdogEnable enables the watchdog timer.
*/

void CFE_PSP_WatchdogDisable(void)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
}
/*
** CFE_PSP_WatchdogDisable disables the watchdog timer.
*/

void CFE_PSP_WatchdogService(void)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
}
/*
** CFE_PSP_WatchdogService services the watchdog timer according to the
** value set in WatchDogSet.
*/

uint32 CFE_PSP_WatchdogGet(void)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}
/*
** CFE_PSP_WatchdogGet gets the watchdog time in milliseconds
*/

void CFE_PSP_WatchdogSet(uint32 WatchdogValue)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
}
/*
** CFE_PSP_WatchdogSet sets the watchdog time in milliseconds
*/

void CFE_PSP_Panic(int32 ErrorCode)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
}
/*
** CFE_PSP_Panic is called by the cFE Core startup code when it needs to abort the
** cFE startup. This should not be called by applications.
*/

int32 CFE_PSP_InitSSR(uint32 bus, uint32 device, char *DeviceName)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}
/*
** CFE_PSP_InitSSR will initialize the Solid state recorder memory for a particular platform
*/

int32 CFE_PSP_Decompress( char * srcFileName, char * dstFileName)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}
/*
** CFE_PSP_Decompress will uncompress the source file to the file specified in the
** destination file name. The Decompress uses the "gzip" algorithm. Files can
** be compressed using the "gzip" program available on almost all host platforms.
*/

void CFE_PSP_AttachExceptions(void)
{
    // For now it is safe for this to do nothing
    // TODO: Actually implement exception handling
}
/*
** CFE_PSP_AttachExceptions will setup the exception environment for the chosen platform
** On a board, this can be configured to look at a debug flag or switch in order to
** keep the standard OS exeption handlers, rather than restarting the system
*/


void CFE_PSP_SetDefaultExceptionEnvironment(void)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
}
/*
**
**   CFE_PSP_SetDefaultExceptionEnvironment defines the CPU and FPU exceptions that are enabled for each cFE Task/App
**
**   Notes: The exception environment is local to each task Therefore this must be
**          called for each task that that wants to do floating point and catch exceptions
*/


/*
** I/O Port API
*/
int32 CFE_PSP_PortRead8(cpuaddr PortAddress, uint8 *ByteValue)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 CFE_PSP_PortWrite8(cpuaddr PortAddress, uint8 ByteValue)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 CFE_PSP_PortRead16(cpuaddr PortAddress, uint16 *uint16Value)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 CFE_PSP_PortWrite16(cpuaddr PortAddress, uint16 uint16Value)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 CFE_PSP_PortRead32(cpuaddr PortAddress, uint32 *uint32Value)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 CFE_PSP_PortWrite32(cpuaddr PortAddress, uint32 uint32Value)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** Memory API
*/
int32 CFE_PSP_MemRead8(cpuaddr MemoryAddress, uint8 *ByteValue)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 CFE_PSP_MemWrite8(cpuaddr MemoryAddress, uint8 ByteValue)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 CFE_PSP_MemRead16(cpuaddr MemoryAddress, uint16 *uint16Value)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 CFE_PSP_MemWrite16(cpuaddr MemoryAddress, uint16 uint16Value)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 CFE_PSP_MemRead32(cpuaddr MemoryAddress, uint32 *uint32Value)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 CFE_PSP_MemWrite32(cpuaddr MemoryAddress, uint32 uint32Value)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 CFE_PSP_MemCpy(void *dest, void *src, uint32 n)
{
    memcpy(dest, src, n);
    return OS_SUCCESS;
}

int32 CFE_PSP_MemSet(void *dest, uint8 value, uint32 n)
{
    memset(dest, value, n);
    return OS_SUCCESS;
}

int32 CFE_PSP_MemValidateRange(cpuaddr Address, uint32 Size, uint32 MemoryType)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

uint32 CFE_PSP_MemRanges(void)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32  CFE_PSP_MemRangeSet(uint32 RangeNum, uint32 MemoryType, cpuaddr StartAddr,
                           uint32 Size, uint32 WordSize, uint32 Attributes)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 CFE_PSP_MemRangeGet (uint32 RangeNum, uint32 *MemoryType, cpuaddr *StartAddr,
                           uint32 *Size,    uint32 *WordSize,   uint32 *Attributes)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 CFE_PSP_EepromWrite8(cpuaddr MemoryAddress, uint8 ByteValue)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 CFE_PSP_EepromWrite16(cpuaddr MemoryAddress, uint16 uint16Value)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 CFE_PSP_EepromWrite32(cpuaddr MemoryAddress, uint32 uint32Value)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 CFE_PSP_EepromWriteEnable(uint32 Bank)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 CFE_PSP_EepromWriteDisable(uint32 Bank)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 CFE_PSP_EepromPowerUp(uint32 Bank)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 CFE_PSP_EepromPowerDown(uint32 Bank)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

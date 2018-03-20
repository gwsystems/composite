#include <string.h>

#include "cFE_util.h"

#include "sl.h"
#include "sl_consts.h"

#include "gen/osapi.h"
#include "gen/cfe_psp.h"


#ifdef _ENHANCED_BUILD_

#include <target_config.h>

/*
 * Define the PSP-supported capacities to be the maximum allowed,
 * (since the PC-linux PSP has the advantage of abundant disk space to hold this)
 */
#define CFE_PSP_CDS_SIZE (GLOBAL_CONFIGDATA.CfeConfig->CdsSize)
#define CFE_PSP_RESET_AREA_SIZE (GLOBAL_CONFIGDATA.CfeConfig->ResetAreaSize)
#define CFE_PSP_USER_RESERVED_SIZE (GLOBAL_CONFIGDATA.CfeConfig->UserReservedSize)

#else

#include "gen/cfe_es.h"           /* For memory sizes */
#include "gen/cfe_platform_cfg.h" /* for processor ID */

#define CFE_PSP_CDS_SIZE (CFE_ES_CDS_SIZE + 1024)
#define CFE_PSP_RESET_AREA_SIZE CFE_ES_RESET_AREA_SIZE
#define CFE_PSP_USER_RESERVED_SIZE CFE_ES_USER_RESERVED_SIZE

#endif


/*
** Function prototypes
*/

/*
** PSP entry point and reset routines
*/
void
CFE_PSP_Main(uint32 ModeId, char *StartupFilePath)
{
	PANIC("Unimplemented method!"); // TODO: Implement me!
}

/*
** CFE_PSP_Main is the entry point that the real time OS calls to start our
** software. This routine will do any BSP/OS specific setup, then call the
** entrypoint of the flight software ( i.e. the cFE main entry point ).
** The flight software (i.e. cFE ) should not call this routine.
*/

void
CFE_PSP_GetTime(OS_time_t *LocalTime)
{
	OS_GetLocalTime(LocalTime);
}
/* This call gets the local time from the hardware on the Vxworks system
 * on the mcp750s
 * on the other os/hardware setup, it will get the time the normal way */


void
CFE_PSP_Restart(uint32 resetType)
{
	PANIC("Unimplemented method!"); // TODO: Implement me!
}
/*
** CFE_PSP_Restart is the entry point back to the BSP to restart the processor.
** The flight software calls this routine to restart the processor.
*/


uint32
CFE_PSP_GetRestartType(uint32 *restartSubType)
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

uint32
CFE_PSP_GetProcessorId(void)
{
	return CFE_PSP_CpuId;
}
/*
** CFE_PSP_GetProcessorId returns the CPU ID as defined by the specific board
** and BSP.
*/


uint32
CFE_PSP_GetSpacecraftId(void)
{
	return CFE_PSP_SpacecraftId;
}
/*
** CFE_PSP_GetSpacecraftId retuns the Spacecraft ID (if any )
*/


uint32
CFE_PSP_Get_Timer_Tick(void)
{
	PANIC("Unimplemented method!"); // TODO: Implement me!
	return 0;
}
/*
** CFE_PSP_Get_Timer_Tick returns the underlying OS timer tick value
** It is used for the performance monitoring software
*/

uint32
CFE_PSP_GetTimerTicksPerSecond(void)
{
	// (usec / sec) / (usec / tick)
	return 1000000 / SL_MIN_PERIOD_US;
}

/*
** CFE_PSP_GetTimerTicksPerSecond provides the resolution of the least significant
** 32 bits of the 64 bit time stamp returned by CFE_PSP_Get_Timebase in timer
** ticks per second.  The timer resolution for accuracy should not be any slower
** than 1000000 ticks per second or 1 us per tick
*/

uint32
CFE_PSP_GetTimerLow32Rollover(void)
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

void
CFE_PSP_Get_Timebase(uint32 *Tbu, uint32 *Tbl)
{
	PANIC("Unimplemented method!"); // TODO: Implement me!
}
/*
** CFE_PSP_Get_Timebase
*/

uint32
CFE_PSP_Get_Dec(void)
{
	PANIC("Unimplemented method!"); // TODO: Implement me!
	return 0;
}
/*
** CFE_PSP_Get_Dec
*/


int32
CFE_PSP_InitProcessorReservedMemory(uint32 RestartType)
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

char CDS_MEMORY[CFE_ES_CDS_SIZE];

int32
CFE_PSP_GetCDSSize(uint32 *SizeOfCDS)
{
	if (!SizeOfCDS) { return CFE_PSP_ERROR; }
	*SizeOfCDS = CFE_ES_CDS_SIZE;
	return CFE_PSP_SUCCESS;
}
/*
** CFE_PSP_GetCDSSize fetches the size of the OS Critical Data Store area.
*/

int32
CFE_PSP_WriteToCDS(void *PtrToDataToWrite, uint32 CDSOffset, uint32 NumBytes)
{
	if (!PtrToDataToWrite) { return CFE_PSP_ERROR; }
	sl_cs_enter();
	memcpy(CDS_MEMORY + CDSOffset, PtrToDataToWrite, NumBytes);
	sl_cs_exit();
	return CFE_PSP_SUCCESS;
}
/*
** CFE_PSP_WriteToCDS writes to the CDS Block.
*/

int32
CFE_PSP_ReadFromCDS(void *PtrToDataToRead, uint32 CDSOffset, uint32 NumBytes)
{
	if (!PtrToDataToRead) { return CFE_PSP_ERROR; }
	sl_cs_enter();
	memcpy(PtrToDataToRead, CDS_MEMORY + CDSOffset, NumBytes);
	sl_cs_exit();
	return CFE_PSP_SUCCESS;
}
/*
** CFE_PSP_ReadFromCDS reads from the CDS Block
*/

// TODO: Make this dynamic based on constants
char RESET_AREA[CFE_PSP_CDS_SIZE];

int32
CFE_PSP_GetResetArea(cpuaddr *PtrToResetArea, uint32 *SizeOfResetArea)
{
	if (!PtrToResetArea || !SizeOfResetArea) { return CFE_PSP_ERROR; }
	*PtrToResetArea  = (cpuaddr)RESET_AREA;
	*SizeOfResetArea = CFE_PSP_CDS_SIZE;
	return CFE_PSP_SUCCESS;
}
/*
** CFE_PSP_GetResetArea returns the location and size of the ES Reset information area.
** This area is preserved during a processor reset and is used to store the
** ER Log, System Log and reset related variables
*/

int32
CFE_PSP_GetUserReservedArea(cpuaddr *PtrToUserArea, uint32 *SizeOfUserArea)
{
	PANIC("Unimplemented method!"); // TODO: Implement me!
	return 0;
}
/*
** CFE_PSP_GetUserReservedArea returns the location and size of the memory used for the cFE
** User reserved area.
*/


int32
CFE_PSP_GetVolatileDiskMem(cpuaddr *PtrToVolDisk, uint32 *SizeOfVolDisk)
{
	if (!PtrToVolDisk || !SizeOfVolDisk) { return CFE_PSP_ERROR; }
	*PtrToVolDisk  = 0;
	*SizeOfVolDisk = 0;
	return CFE_PSP_SUCCESS;
}
/*
** CFE_PSP_GetVolatileDiskMem returns the location and size of the memory used for the cFE
** volatile disk.
*/

int32
CFE_PSP_GetKernelTextSegmentInfo(cpuaddr *PtrToKernelSegment, uint32 *SizeOfKernelSegment)
{
	// TODO: Verify this is ok to remain unimplemented
	return CFE_PSP_ERROR_NOT_IMPLEMENTED;
}
/*
** CFE_PSP_GetKernelTextSegmentInfo returns the location and size of the kernel memory.
*/

int32
CFE_PSP_GetCFETextSegmentInfo(cpuaddr *PtrToCFESegment, uint32 *SizeOfCFESegment)
{
	// TODO: Verify this is ok to remain unimplemented
	return CFE_PSP_ERROR_NOT_IMPLEMENTED;
}
/*
** CFE_PSP_GetCFETextSegmentInfo returns the location and size of the kernel memory.
*/

void
CFE_PSP_WatchdogInit(void)
{
	PANIC("Unimplemented method!"); // TODO: Implement me!
}
/*
** CFE_PSP_WatchdogInit configures the watchdog timer.
*/

void
CFE_PSP_WatchdogEnable(void)
{
	PANIC("Unimplemented method!"); // TODO: Implement me!
}
/*
** CFE_PSP_WatchdogEnable enables the watchdog timer.
*/

void
CFE_PSP_WatchdogDisable(void)
{
	PANIC("Unimplemented method!"); // TODO: Implement me!
}
/*
** CFE_PSP_WatchdogDisable disables the watchdog timer.
*/

void
CFE_PSP_WatchdogService(void)
{
	PANIC("Unimplemented method!"); // TODO: Implement me!
}
/*
** CFE_PSP_WatchdogService services the watchdog timer according to the
** value set in WatchDogSet.
*/

uint32
CFE_PSP_WatchdogGet(void)
{
	PANIC("Unimplemented method!"); // TODO: Implement me!
	return 0;
}
/*
** CFE_PSP_WatchdogGet gets the watchdog time in milliseconds
*/

void
CFE_PSP_WatchdogSet(uint32 WatchdogValue)
{
	PANIC("Unimplemented method!"); // TODO: Implement me!
}
/*
** CFE_PSP_WatchdogSet sets the watchdog time in milliseconds
*/

void
CFE_PSP_Panic(int32 ErrorCode)
{
	printc("Error code is %d\n", (int)ErrorCode);
	PANIC("PANICKING!");
}
/*
** CFE_PSP_Panic is called by the cFE Core startup code when it needs to abort the
** cFE startup. This should not be called by applications.
*/

int32
CFE_PSP_InitSSR(uint32 bus, uint32 device, char *DeviceName)
{
	PANIC("Unimplemented method!"); // TODO: Implement me!
	return 0;
}
/*
** CFE_PSP_InitSSR will initialize the Solid state recorder memory for a particular platform
*/

int32
CFE_PSP_Decompress(char *srcFileName, char *dstFileName)
{
	PANIC("Unimplemented method!"); // TODO: Implement me!
	return 0;
}
/*
** CFE_PSP_Decompress will uncompress the source file to the file specified in the
** destination file name. The Decompress uses the "gzip" algorithm. Files can
** be compressed using the "gzip" program available on almost all host platforms.
*/

void
CFE_PSP_AttachExceptions(void)
{
	// For now it is safe for this to do nothing
	// TODO: Actually implement exception handling
}
/*
** CFE_PSP_AttachExceptions will setup the exception environment for the chosen platform
** On a board, this can be configured to look at a debug flag or switch in order to
** keep the standard OS exeption handlers, rather than restarting the system
*/


void
CFE_PSP_SetDefaultExceptionEnvironment(void)
{
	// TODO: Figure out if it is safe for us to just do nothing here
}
/*
**
**   CFE_PSP_SetDefaultExceptionEnvironment defines the CPU and FPU exceptions that are enabled for each cFE Task/App
**
**   Notes: The exception environment is local to each task Therefore this must be
**          called for each task that that wants to do floating point and catch exceptions
*/

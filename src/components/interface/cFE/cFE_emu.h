#include <cos_types.h>

#include <cfe_evs.h>
#include <cfe_fs.h>

#define EMU_BUF_SIZE 512

/* TODO: Alphabetize me! */
union shared_region {
	struct {
		CFE_EVS_BinFilter_t filters[CFE_EVS_MAX_EVENT_FILTERS];
		uint16              NumEventFilters;
		uint16              FilterScheme;
	} cfe_evs_register;
	struct {
		CFE_SB_PipeId_t PipeId;
		uint16          Depth;
		char            PipeName[OS_MAX_API_NAME];
	} cfe_sb_createPipe;
	struct {
		char           MsgBuffer[EMU_BUF_SIZE];
		CFE_SB_MsgId_t MsgId;
		uint16         Length;
		boolean        Clear;
	} cfe_sb_initMsg;
	struct {
		char   Msg[EMU_BUF_SIZE];
		uint16 EventID;
		uint16 EventType;
	} cfe_evs_sendEvent;
	struct {
		uint32 RunStatus;
	} cfe_es_runLoop;
	struct {
		CFE_SB_PipeId_t PipeId;
		int32           TimeOut;
		char            Msg[EMU_BUF_SIZE];
	} cfe_sb_rcvMsg;
	struct {
		CFE_SB_Msg_t Msg;
	} cfe_sb_getMsgLen;
	struct {
		char Msg[EMU_BUF_SIZE];
	} cfe_sb_msg;
	CFE_TIME_SysTime_t time;
	struct {
		char PrintBuffer[CFE_TIME_PRINTED_STRING_SIZE];
		CFE_TIME_SysTime_t TimeToPrint;
	} cfe_time_print;
	struct {
		char Msg[EMU_BUF_SIZE];
		uint16 CmdCode;
	} cfe_sb_setCmdCode;
	struct {
		CFE_TIME_SysTime_t Time1;
		CFE_TIME_SysTime_t Time2;
		CFE_TIME_SysTime_t Result;
	} cfe_time_add;
	struct {
		CFE_TIME_SysTime_t Time1;
		CFE_TIME_SysTime_t Time2;
		CFE_TIME_Compare_t Result;
	} cfe_time_compare;
	struct {
		CFE_ES_TaskInfo_t TaskInfo;
		uint32 TaskId;
	} cfe_es_getTaskInfo;
	struct {
		uint32 ResetSubtype;
	} cfe_es_getResetType;
	struct {
		uint32 CounterId;
		uint32 Count;
	} cfe_es_getGenCount;
	struct {
		uint32 CounterId;
		char CounterName[EMU_BUF_SIZE];
	} cfe_es_getGenCounterIDByName;
	struct {
		int32 FileDes;
		CFE_FS_Header_t Hdr;
	} cfe_fs_writeHeader;
	struct {
		char SourceFile[EMU_BUF_SIZE];
		char DestinationFile[EMU_BUF_SIZE];
	} cfe_fs_decompress;
	struct {
		char path[EMU_BUF_SIZE];
		int32 access;
	} os_creat;
	struct {
		int32 filedes;
		char buffer[EMU_BUF_SIZE];
		uint32 nbytes;
	} os_write;
	struct {
		cpuaddr symbol_address;
		char symbol_name[EMU_BUF_SIZE];
	} os_symbolLookup;
	struct {
		uint32 sem_id;
		char sem_name[EMU_BUF_SIZE];
		uint32 sem_initial_value;
		uint32 options;
	} os_semCreate;
	struct {
		char src[EMU_BUF_SIZE];
		char dest[EMU_BUF_SIZE];
	} os_cp;
	struct {
		int32 filedes;
		OS_FDTableEntry fd_prop;
	} os_FDGetInfo;
	struct {
		char name[EMU_BUF_SIZE];
		uint64 bytes_free;
	} os_fsBytesFree;
	struct {
		char path[EMU_BUF_SIZE];
		uint32 access;
	} os_mkdir;
	struct {
		uint32 sem_id;
		char sem_name[EMU_BUF_SIZE];
		uint32 options;
	} os_mutSemCreate;
	struct {
		char src[EMU_BUF_SIZE];
		char dest[EMU_BUF_SIZE];
	} os_mv;
	struct {
		char path[EMU_BUF_SIZE];
	} os_opendir;
	struct {
		int32 filedes;
		char buffer[EMU_BUF_SIZE];
		uint32 nbytes;
	} os_read;
	struct {
		char path[EMU_BUF_SIZE];
	} os_remove;
	struct {
		char old_filename[EMU_BUF_SIZE];
		char new_filename[EMU_BUF_SIZE];
	} os_rename;
	struct {
		char path[EMU_BUF_SIZE];
	} os_rmdir;
	struct {
		char path[EMU_BUF_SIZE];
		os_fstat_t filestats;
	} os_stat;
	struct {
		os_dirp_t directory;
		os_dirent_t dirent;
	} os_readdir;
	struct {
		uint32 task_id;
		char task_name[EMU_BUF_SIZE];
	} os_taskGetIdByName;
	struct {
		char String[EMU_BUF_SIZE];
	} cfe_es_writeToSysLog;
	struct {
		char path[EMU_BUF_SIZE];
		int32 access;
		uint32 mode;
	} os_open;
	struct {
		char Data[EMU_BUF_SIZE];
		uint32 DataLength;
		uint32 InputCRC;
		uint32 TypeCRC;
	} cfe_es_calculateCRC;
	struct {
		uint32 AppId;
		char AppName[EMU_BUF_SIZE];
	} cfe_es_getAppIDByName;
	struct {
		CFE_ES_AppInfo_t AppInfo;
		uint32 AppId;
	} cfe_es_getAppInfo;
	struct {
		CFE_SB_MsgId_t MsgId;
		CFE_SB_PipeId_t PipeId;
		CFE_SB_Qos_t Quality;
		uint16 MsgLim;
	} cfe_sb_subscribeEx;
	struct {
		CFE_ES_CDSHandle_t CDS_Handle;
		int32 BlockSize;
		char Name[EMU_BUF_SIZE];
	} cfe_es_registerCDS;
	struct {
		CFE_ES_CDSHandle_t CDSHandle;
		char DataToCopy[EMU_BUF_SIZE];
	} cfe_es_copyToCDS;
	struct {
		CFE_ES_CDSHandle_t CDSHandle;
		char RestoreToMemory[EMU_BUF_SIZE];
	} cfe_es_restoreFromCDS;
};

int emu_backend_request_memory(spdid_t client);

int32 emu_CFE_ES_CalculateCRC(spdid_t client);
int32 emu_CFE_ES_CopyToCDS(spdid_t client);
int32 emu_CFE_ES_GetAppIDByName(spdid_t client);
int32 emu_CFE_ES_GetAppInfo(spdid_t client);
int32 emu_CFE_ES_GetGenCount(spdid_t client);
int32 emu_CFE_ES_GetGenCounterIDByName(spdid_t client);
int32 emu_CFE_ES_GetResetType(spdid_t client);
int32 emu_CFE_ES_GetTaskInfo(spdid_t client);
int32 emu_CFE_ES_RegisterCDS(spdid_t client);
int32 emu_CFE_ES_RestoreFromCDS(spdid_t client);
int32 emu_CFE_ES_RunLoop(spdid_t client);
int32 emu_CFE_ES_WriteToSysLog(spdid_t client);

int32 emu_CFE_EVS_Register(spdid_t sp);
int32 emu_CFE_EVS_SendEvent(spdid_t client);

int32 emu_CFE_FS_Decompress(spdid_t client);
int32 emu_CFE_FS_WriteHeader(spdid_t client);

int32 emu_CFE_SB_CreatePipe(spdid_t client);
uint16 emu_CFE_SB_GetCmdCode(spdid_t client);
CFE_SB_MsgId_t emu_CFE_SB_GetMsgId(spdid_t client);
void emu_CFE_SB_GetMsgTime(spdid_t client);
uint16 emu_CFE_SB_GetTotalMsgLength(spdid_t client);
void emu_CFE_SB_InitMsg(spdid_t client);
int32 emu_CFE_SB_RcvMsg(spdid_t client);
int32 emu_CFE_SB_SetCmdCode(spdid_t client);
int32 emu_CFE_SB_SendMsg(spdid_t client);
int32 emu_CFE_SB_SubscribeEx(spdid_t client);
void emu_CFE_SB_TimeStampMsg(spdid_t client);
boolean emu_CFE_SB_ValidateChecksum(spdid_t client);

void emu_CFE_TIME_Add(spdid_t client);
void emu_CFE_TIME_Compare(spdid_t client);
void emu_CFE_TIME_GetTime(spdid_t client);
void emu_CFE_TIME_Print(spdid_t client);

int32 emu_OS_cp(spdid_t client);
int32 emu_OS_creat(spdid_t client);
int32 emu_OS_FDGetInfo(spdid_t client);
int32 emu_OS_fsBytesFree(spdid_t client);
int32 emu_OS_mkdir(spdid_t client);
int32 emu_OS_mv(spdid_t client);
int32 emu_OS_open(spdid_t client);
os_dirp_t emu_OS_opendir(spdid_t client);
int32 emu_OS_read(spdid_t client);
void emu_OS_readdir(spdid_t client);
int32 emu_OS_remove(spdid_t client);
int32 emu_OS_rename(spdid_t client);
int32 emu_OS_rmdir(spdid_t client);
int32 emu_OS_stat(spdid_t client);
int32 emu_OS_write(spdid_t client);

int32 emu_OS_BinSemCreate(spdid_t client);
int32 emu_OS_CountSemCreate(spdid_t client);
int32 emu_OS_MutSemCreate(spdid_t client);
int32 emu_OS_TaskGetIdByName(spdid_t client);

int32 emu_OS_SymbolLookup(spdid_t client);

#include <stdarg.h>

#include <cos_component.h>
#include <cos_debug.h>
#include <cos_kernel_api.h>
#include <cos_thd_init.h>
#include <llprint.h>
#include <sl.h>
#include "../interface/capmgr/memmgr.h"

#include <cFE_emu.h>

#define BASE_AEP_KEY 0xBEAFCAFE

union shared_region *shared_region;
spdid_t              spdid;
cos_aepkey_t         time_sync_key;

/* We could copy this from the cFE, but it's zero intialized there
 * Also it's totally unused (according to cFE documentation)
 */
CFE_SB_Qos_t CFE_SB_Default_Qos = {0};

size_t CDS_sizes[CFE_ES_CDS_MAX_NUM_ENTRIES] = {0};

int                         sync_callbacks_are_setup = 0;
arcvcap_t                   sync_callback_rcv = 0;
CFE_TIME_SynchCallbackPtr_t sync_callbacks[CFE_TIME_MAX_NUM_SYNCH_FUNCS];

void
do_emulation_setup(spdid_t id)
{
	vaddr_t client_addr;
	int     region_id = emu_request_memory(id);

	spdid = id;

	memmgr_shared_page_map(region_id, &client_addr);
	assert(client_addr);
	shared_region = (void *)client_addr;

	time_sync_key = BASE_AEP_KEY + id;

	/* End with a quick consistency check */
	assert(sizeof(union shared_region) <= SHARED_REGION_NUM_PAGES * PAGE_SIZE);
}

void
handle_sync_callbacks(void *data)
{
	int pending;

	while (1) {
		assert(sync_callback_rcv);
		pending = cos_rcv(sync_callback_rcv, 0, NULL);
		if (pending) {
			int i;
			for (i = 0; i < CFE_TIME_MAX_NUM_SYNCH_FUNCS; i++) {
				if (sync_callbacks[i]) { sync_callbacks[i](); }
			}
		}
	}
}

void
ensure_sync_callbacks_are_setup()
{
	if (!sync_callbacks_are_setup) {
		int32              result;
		thdclosure_index_t idx = cos_thd_init_alloc(handle_sync_callbacks, NULL);
		sync_callback_rcv = emu_create_aep_thread(spdid, idx, time_sync_key);

		result = emu_CFE_TIME_RegisterSynchCallback(time_sync_key);
		assert(result == CFE_SUCCESS);
		sync_callbacks_are_setup = 1;
	}
}


/* FIXME: Be more careful about user supplied pointers
 * FIXME: Take a lock in each function, so shared memory can't be corrupted
 * FIXME: Don't pass spdid, use the builtin functionality instead
 */
uint32
CFE_ES_CalculateCRC(const void *DataPtr, uint32 DataLength, uint32 InputCRC, uint32 TypeCRC)
{
	assert(DataLength < EMU_BUF_SIZE);
	memcpy(shared_region->cfe_es_calculateCRC.Data, DataPtr, DataLength);
	shared_region->cfe_es_calculateCRC.InputCRC = InputCRC;
	shared_region->cfe_es_calculateCRC.TypeCRC  = TypeCRC;
	return emu_CFE_ES_CalculateCRC(spdid);
}

int32
CFE_ES_CopyToCDS(CFE_ES_CDSHandle_t CDSHandle, void *DataToCopy)
{
	size_t data_size;

	/* CDSHandle is unsigned, so it can't be invalid by being negative */
	assert(CDSHandle < CFE_ES_CDS_MAX_NUM_ENTRIES);

	shared_region->cfe_es_copyToCDS.CDSHandle = CDSHandle;

	data_size = CDS_sizes[CDSHandle];
	assert(data_size <= EMU_BUF_SIZE);
	memcpy(shared_region->cfe_es_copyToCDS.DataToCopy, DataToCopy, data_size);
	return emu_CFE_ES_CopyToCDS(spdid);
}

int32
CFE_ES_CreateChildTask(uint32 *TaskIdPtr, const char *TaskName, CFE_ES_ChildTaskMainFuncPtr_t FunctionPtr,
                       uint32 *StackPtr, uint32 StackSize, uint32 Priority, uint32 Flags)
{
	int32              result;
	thdclosure_index_t idx = cos_thd_init_alloc(FunctionPtr, NULL);

	assert(strlen(TaskName) < EMU_BUF_SIZE);
	strcpy(shared_region->cfe_es_createChildTask.TaskName, TaskName);

	emu_stash(idx, spdid);

	shared_region->cfe_es_createChildTask.FunctionPtr = STASH_MAGIC_VALUE;
	shared_region->cfe_es_createChildTask.Priority    = Priority;
	shared_region->cfe_es_createChildTask.Flags       = Flags;

	result     = emu_CFE_ES_CreateChildTask(spdid);
	*TaskIdPtr = shared_region->cfe_es_createChildTask.TaskId;

	emu_stash_clear();

	return result;
}

int32
CFE_ES_GetAppIDByName(uint32 *AppIdPtr, const char *AppName)
{
	int32 result;

	assert(strlen(AppName) < EMU_BUF_SIZE);

	strcpy(shared_region->cfe_es_getAppIDByName.AppName, AppName);
	result    = emu_CFE_ES_GetAppIDByName(spdid);
	*AppIdPtr = shared_region->cfe_es_getAppIDByName.AppId;

	return result;
}

int32
CFE_ES_GetAppInfo(CFE_ES_AppInfo_t *AppInfo, uint32 AppId)
{
	int32 result;

	shared_region->cfe_es_getAppInfo.AppId = AppId;
	result                                 = emu_CFE_ES_GetAppInfo(spdid);
	*AppInfo                               = shared_region->cfe_es_getAppInfo.AppInfo;

	return result;
}

int32
CFE_ES_GetGenCount(uint32 CounterId, uint32 *Count)
{
	int32 result;

	shared_region->cfe_es_getGenCount.CounterId = CounterId;
	result                                      = emu_CFE_ES_GetGenCount(spdid);
	*Count                                      = shared_region->cfe_es_getGenCount.Count;

	return result;
}

int32
CFE_ES_GetGenCounterIDByName(uint32 *CounterIdPtr, const char *CounterName)
{
	int32 result;

	strcpy(shared_region->cfe_es_getGenCounterIDByName.CounterName, CounterName);
	result        = emu_CFE_ES_GetGenCounterIDByName(spdid);
	*CounterIdPtr = shared_region->cfe_es_getGenCounterIDByName.CounterId;

	return result;
}

int32
CFE_ES_GetResetType(uint32 *ResetSubtypePtr)
{
	int32 result;

	result = emu_CFE_ES_GetResetType(spdid);

	if (ResetSubtypePtr) {
		*ResetSubtypePtr = shared_region->cfe_es_getResetType.ResetSubtype;
	}

	return result;
}

int32
CFE_ES_GetTaskInfo(CFE_ES_TaskInfo_t *TaskInfo, uint32 TaskId)
{
	int32 result;

	shared_region->cfe_es_getTaskInfo.TaskId = TaskId;
	result                                   = emu_CFE_ES_GetTaskInfo(spdid);
	*TaskInfo                                = shared_region->cfe_es_getTaskInfo.TaskInfo;
	return result;
}

int32
CFE_ES_RegisterCDS(CFE_ES_CDSHandle_t *HandlePtr, int32 BlockSize, const char *Name)
{
	int32 result;

	assert(strlen(Name) < EMU_BUF_SIZE);

	shared_region->cfe_es_registerCDS.BlockSize = BlockSize;
	strcpy(shared_region->cfe_es_registerCDS.Name, Name);

	result = emu_CFE_ES_RegisterCDS(spdid);
	if (result == CFE_SUCCESS) {
		CFE_ES_CDSHandle_t handle = shared_region->cfe_es_registerCDS.CDS_Handle;
		CDS_sizes[handle]         = BlockSize;
		*HandlePtr                = handle;
	}
	return result;
}

int32
CFE_ES_RestoreFromCDS(void *RestoreToMemory, CFE_ES_CDSHandle_t CDSHandle)
{
	int32  result;
	size_t data_size;

	shared_region->cfe_es_restoreFromCDS.CDSHandle = CDSHandle;

	result = emu_CFE_ES_RestoreFromCDS(spdid);

	if (result == CFE_SUCCESS) {
		/* CDSHandle is unsigned, so it can't be invalid by being negative */
		assert(CDSHandle < CFE_ES_CDS_MAX_NUM_ENTRIES);
		data_size = CDS_sizes[CDSHandle];
		assert(data_size <= EMU_BUF_SIZE);

		memcpy(RestoreToMemory, shared_region->cfe_es_restoreFromCDS.RestoreToMemory, data_size);
	}

	return result;
}

int32
CFE_ES_RunLoop(uint32 *RunStatus)
{
	int32 result;

	shared_region->cfe_es_runLoop.RunStatus = *RunStatus;
	result                                  = emu_CFE_ES_RunLoop(spdid);
	*RunStatus                              = shared_region->cfe_es_runLoop.RunStatus;
	return result;
}

int32
CFE_ES_WriteToSysLog(const char *SpecStringPtr, ...)
{
	va_list arg_ptr;
	int     ret, len = OS_BUFFER_SIZE;

	va_start(arg_ptr, SpecStringPtr);
	vsnprintf(shared_region->cfe_es_writeToSysLog.String, len, SpecStringPtr, arg_ptr);
	va_end(arg_ptr);
	return emu_CFE_ES_WriteToSysLog(spdid);
}

int32 CFE_EVS_Register(void * Filters,           /* Pointer to an array of filters */
                       uint16 NumFilteredEvents, /* How many elements in the array? */
                       uint16 FilterScheme)      /* Filtering Algorithm to be implemented */
{
	int                  i;
	CFE_EVS_BinFilter_t *bin_filters;

	if (FilterScheme != CFE_EVS_BINARY_FILTER) { return CFE_EVS_UNKNOWN_FILTER; }
	bin_filters = Filters;

	for (i = 0; i < NumFilteredEvents; i++) { shared_region->cfe_evs_register.filters[i] = bin_filters[i]; }

	shared_region->cfe_evs_register.NumEventFilters = NumFilteredEvents;
	shared_region->cfe_evs_register.FilterScheme    = FilterScheme;

	return emu_CFE_EVS_Register(spdid);
}


int32
CFE_EVS_SendEvent(uint16 EventID, uint16 EventType, const char *Spec, ...)
{
	va_list Ptr;
	va_start(Ptr, Spec);
	vsnprintf(shared_region->cfe_evs_sendEvent.Msg, sizeof(shared_region->cfe_evs_sendEvent.Msg), Spec, Ptr);
	va_end(Ptr);

	shared_region->cfe_evs_sendEvent.EventID   = EventID;
	shared_region->cfe_evs_sendEvent.EventType = EventType;

	return emu_CFE_EVS_SendEvent(spdid);
}

int32
CFE_FS_Decompress(const char *SourceFile, const char *DestinationFile)
{
	assert(strlen(SourceFile) < EMU_BUF_SIZE);
	assert(strlen(DestinationFile) < EMU_BUF_SIZE);

	strcpy(shared_region->cfe_fs_decompress.SourceFile, SourceFile);
	strcpy(shared_region->cfe_fs_decompress.DestinationFile, DestinationFile);
	return emu_CFE_FS_Decompress(spdid);
}

int32
CFE_FS_ReadHeader(CFE_FS_Header_t *Hdr, int32 FileDes)
{
	int32 result;

	shared_region->cfe_fs_readHeader.FileDes = FileDes;

	result = emu_CFE_FS_ReadHeader(spdid);
	*Hdr = shared_region->cfe_fs_readHeader.Hdr;

	return result;
}

int32
CFE_FS_WriteHeader(int32 FileDes, CFE_FS_Header_t *Hdr)
{
	int32 result;
	shared_region->cfe_fs_writeHeader.FileDes = FileDes;
	result                                    = emu_CFE_FS_WriteHeader(spdid);
	*Hdr                                      = shared_region->cfe_fs_writeHeader.Hdr;
	return result;
}

int32
CFE_PSP_MemCpy(void *dest, void *src, uint32 n)
{
	memcpy(dest, src, n);
	return CFE_PSP_SUCCESS;
}

int32
CFE_PSP_MemRead8(cpuaddr MemoryAddress, uint8 *ByteValue)
{

	*ByteValue = *((uint8 *)MemoryAddress) ;

	return CFE_PSP_SUCCESS;
}

int32
CFE_PSP_MemRead16(cpuaddr MemoryAddress, uint16 *uint16Value)
{
	/* check 16 bit alignment */
	if (MemoryAddress & 0x00000001) {
		return CFE_PSP_ERROR_ADDRESS_MISALIGNED;
	}
	*uint16Value = *((uint16 *)MemoryAddress) ;
	return CFE_PSP_SUCCESS;
}

int32
CFE_PSP_MemRead32(cpuaddr MemoryAddress, uint32 *uint32Value)
{
	/* check 32 bit alignment  */
	if (MemoryAddress & 0x00000003) {
		return CFE_PSP_ERROR_ADDRESS_MISALIGNED;
	}
	*uint32Value = *((uint32 *)MemoryAddress);

	return CFE_PSP_SUCCESS;
}

int32
CFE_PSP_MemSet(void *dest, uint8 value, uint32 n)
{
	memset(dest, value, n);
	return CFE_PSP_SUCCESS;
}


int32
CFE_PSP_MemWrite8(cpuaddr MemoryAddress, uint8 ByteValue)
{
    *((uint8 *)MemoryAddress) = ByteValue;
	return CFE_PSP_SUCCESS;

}

int32
CFE_PSP_MemWrite16(cpuaddr MemoryAddress, uint16 uint16Value )
{
	/* check 16 bit alignment  , check the 1st lsb */
	if (MemoryAddress & 0x00000001) {
		return CFE_PSP_ERROR_ADDRESS_MISALIGNED;
	}
	*((uint16 *)MemoryAddress) = uint16Value;
	return CFE_PSP_SUCCESS;
}

int32
CFE_PSP_MemWrite32(cpuaddr MemoryAddress, uint32 uint32Value)
{
	/* check 32 bit alignment  */
	if (MemoryAddress & 0x00000003) {
		return CFE_PSP_ERROR_ADDRESS_MISALIGNED;
	}

	*((uint32 *)MemoryAddress) = uint32Value;

	return CFE_PSP_SUCCESS;
}

int32
CFE_SB_CreatePipe(CFE_SB_PipeId_t *PipeIdPtr, uint16 Depth, const char *PipeName)
{
	int32 result;

	shared_region->cfe_sb_createPipe.Depth = Depth;
	strncpy(shared_region->cfe_sb_createPipe.PipeName, PipeName, OS_MAX_API_NAME);
	result     = emu_CFE_SB_CreatePipe(spdid);
	*PipeIdPtr = shared_region->cfe_sb_createPipe.PipeId;
	return result;
}

uint16
CFE_SB_GetCmdCode(CFE_SB_MsgPtr_t MsgPtr)
{
	char * msg_ptr;
	uint16 msg_len = CFE_SB_GetTotalMsgLength(MsgPtr);

	assert(msg_len <= EMU_BUF_SIZE);
	msg_ptr = (char *)MsgPtr;
	memcpy(shared_region->cfe_sb_msg.Msg, msg_ptr, (size_t)msg_len);
	return emu_CFE_SB_GetCmdCode(spdid);
}

CFE_SB_MsgId_t
CFE_SB_GetMsgId(CFE_SB_MsgPtr_t MsgPtr)
{
	char * msg_ptr;
	uint16 msg_len = CFE_SB_GetTotalMsgLength(MsgPtr);

	assert(msg_len <= EMU_BUF_SIZE);
	msg_ptr = (char *)MsgPtr;
	memcpy(shared_region->cfe_sb_msg.Msg, msg_ptr, (size_t)msg_len);
	return emu_CFE_SB_GetMsgId(spdid);
}


CFE_TIME_SysTime_t
CFE_SB_GetMsgTime(CFE_SB_MsgPtr_t MsgPtr)
{
	char * msg_ptr;
	uint16 msg_len = CFE_SB_GetTotalMsgLength(MsgPtr);

	assert(msg_len <= EMU_BUF_SIZE);
	msg_ptr = (char *)MsgPtr;
	memcpy(shared_region->cfe_sb_msg.Msg, msg_ptr, (size_t)msg_len);
	emu_CFE_SB_GetMsgTime(spdid);
	return shared_region->time;
}

uint16
CFE_SB_GetTotalMsgLength(CFE_SB_MsgPtr_t MsgPtr)
{
	shared_region->cfe_sb_getMsgLen.Msg = *MsgPtr;
	return emu_CFE_SB_GetTotalMsgLength(spdid);
}

void
CFE_SB_InitMsg(void *MsgPtr, CFE_SB_MsgId_t MsgId, uint16 Length, boolean Clear)
{
	char *source = MsgPtr;
	assert(Length <= EMU_BUF_SIZE);
	memcpy(shared_region->cfe_sb_initMsg.MsgBuffer, source, Length);
	shared_region->cfe_sb_initMsg.MsgId  = MsgId;
	shared_region->cfe_sb_initMsg.Length = Length;
	shared_region->cfe_sb_initMsg.Clear  = Clear;
	emu_CFE_SB_InitMsg(spdid);
	memcpy(source, shared_region->cfe_sb_initMsg.MsgBuffer, Length);
}

/*
 * We want the msg to live in this app, not the cFE component
 * But the message is stored in a buffer on the cFE side
 * The slution is to copy it into a buffer here
 * According to the cFE spec, this buffer only needs to last till the pipe is used again
 * Therefore one buffer per pipe is acceptable
 */
struct {
	char buf[EMU_BUF_SIZE];
} pipe_buffers[CFE_SB_MAX_PIPES];

int32
CFE_SB_RcvMsg(CFE_SB_MsgPtr_t *BufPtr, CFE_SB_PipeId_t PipeId, int32 TimeOut)
{
	int32 result;

	shared_region->cfe_sb_rcvMsg.PipeId  = PipeId;
	shared_region->cfe_sb_rcvMsg.TimeOut = TimeOut;
	result                               = emu_CFE_SB_RcvMsg(spdid);

	if (result == CFE_SUCCESS) {
		memcpy(pipe_buffers[PipeId].buf, shared_region->cfe_sb_rcvMsg.Msg, EMU_BUF_SIZE);
		*BufPtr = (CFE_SB_MsgPtr_t)pipe_buffers[PipeId].buf;
	}

	return result;
}

int32
CFE_SB_SetCmdCode(CFE_SB_MsgPtr_t MsgPtr, uint16 CmdCode)
{
	int32  result;
	char * msg_ptr;
	uint16 msg_len = CFE_SB_GetTotalMsgLength(MsgPtr);

	assert(msg_len <= EMU_BUF_SIZE);
	msg_ptr = (char *)MsgPtr;

	memcpy(shared_region->cfe_sb_setCmdCode.Msg, msg_ptr, (size_t)msg_len);
	shared_region->cfe_sb_setCmdCode.CmdCode = CmdCode;

	result = emu_CFE_SB_SetCmdCode(spdid);
	/* TODO: Verify we can assume the msg_len won't change */
	memcpy(msg_ptr, shared_region->cfe_sb_setCmdCode.Msg, msg_len);
	return result;
}

int32
CFE_SB_SendMsg(CFE_SB_Msg_t *MsgPtr)
{
	char * msg_ptr;
	uint16 msg_len = CFE_SB_GetTotalMsgLength(MsgPtr);

	assert(msg_len <= EMU_BUF_SIZE);
	msg_ptr = (char *)MsgPtr;
	memcpy(shared_region->cfe_sb_msg.Msg, msg_ptr, (size_t)msg_len);

	return emu_CFE_SB_SendMsg(spdid);
}

int32
CFE_SB_SubscribeEx(CFE_SB_MsgId_t MsgId, CFE_SB_PipeId_t PipeId, CFE_SB_Qos_t Quality, uint16 MsgLim)
{
	shared_region->cfe_sb_subscribeEx.MsgId   = MsgId;
	shared_region->cfe_sb_subscribeEx.PipeId  = PipeId;
	shared_region->cfe_sb_subscribeEx.Quality = Quality;
	shared_region->cfe_sb_subscribeEx.MsgLim  = MsgLim;
	return emu_CFE_SB_SubscribeEx(spdid);
}

void
CFE_SB_TimeStampMsg(CFE_SB_MsgPtr_t MsgPtr)
{
	char * msg_ptr;
	uint16 msg_len = CFE_SB_GetTotalMsgLength(MsgPtr);

	assert(msg_len <= EMU_BUF_SIZE);
	msg_ptr = (char *)MsgPtr;
	memcpy(shared_region->cfe_sb_msg.Msg, msg_ptr, (size_t)msg_len);
	emu_CFE_SB_TimeStampMsg(spdid);
	memcpy(msg_ptr, shared_region->cfe_sb_msg.Msg, (size_t)msg_len);
}

boolean
CFE_SB_ValidateChecksum(CFE_SB_MsgPtr_t MsgPtr)
{
	char * msg_ptr;
	uint16 msg_len = CFE_SB_GetTotalMsgLength(MsgPtr);

	assert(msg_len <= EMU_BUF_SIZE);
	msg_ptr = (char *)MsgPtr;
	memcpy(shared_region->cfe_sb_msg.Msg, msg_ptr, (size_t)msg_len);

	return emu_CFE_SB_ValidateChecksum(spdid);
}

struct {
	size_t size;
	char   buffer[EMU_TBL_BUF_SIZE];
} table_info[CFE_TBL_MAX_NUM_HANDLES];

int32
CFE_TBL_GetAddress(void **TblPtr, CFE_TBL_Handle_t TblHandle)
{
	int32 result;

	shared_region->cfe_tbl_getAddress.TblHandle = TblHandle;

	result = emu_CFE_TBL_GetAddress(spdid);

	if (result == CFE_SUCCESS || result == CFE_TBL_INFO_UPDATED) {
		memcpy(table_info[TblHandle].buffer, shared_region->cfe_tbl_getAddress.Buffer,
		       table_info[TblHandle].size);

		*TblPtr = table_info[TblHandle].buffer;
	}

	return result;
}

int32
CFE_TBL_GetInfo(CFE_TBL_Info_t *TblInfoPtr, const char *TblName)
{
	int32 result;

	assert(strlen(TblName) < EMU_BUF_SIZE);
	strcpy(shared_region->cfe_tbl_getInfo.TblName, TblName);

	result = emu_CFE_TBL_GetInfo(spdid);
	if (result == CFE_SUCCESS) { *TblInfoPtr = shared_region->cfe_tbl_getInfo.TblInfo; }
	return result;
}

int32
CFE_TBL_Load(CFE_TBL_Handle_t TblHandle, CFE_TBL_SrcEnum_t SrcType, const void *SrcDataPtr)
{
	shared_region->cfe_tbl_load.TblHandle = TblHandle;
	shared_region->cfe_tbl_load.SrcType   = SrcType;

	if (SrcType == CFE_TBL_SRC_FILE) {
		assert(strlen(SrcDataPtr) < EMU_TBL_BUF_SIZE);
		strcpy(shared_region->cfe_tbl_load.SrcData, SrcDataPtr);
	} else if (SrcType == CFE_TBL_SRC_ADDRESS) {
		assert(TblHandle < CFE_TBL_MAX_NUM_HANDLES);
		memcpy(shared_region->cfe_tbl_load.SrcData, SrcDataPtr, table_info[TblHandle].size);
	} else {
		return CFE_TBL_ERR_ILLEGAL_SRC_TYPE;
	}
	return emu_CFE_TBL_Load(spdid);
}

int32
CFE_TBL_Modified(CFE_TBL_Handle_t TblHandle)
{
	assert(TblHandle < CFE_TBL_MAX_NUM_HANDLES);
	memcpy(shared_region->cfe_tbl_modified.Buffer, table_info[TblHandle].buffer, table_info[TblHandle].size);

	shared_region->cfe_tbl_modified.TblHandle = TblHandle;

	return emu_CFE_TBL_Modified(spdid);
}

int32
CFE_TBL_Register(CFE_TBL_Handle_t *TblHandlePtr, const char *Name, uint32 TblSize, uint16 TblOptionFlags,
                 CFE_TBL_CallbackFuncPtr_t TblValidationFuncPtr)
{
	int32 result;

	assert(strlen(Name) < EMU_BUF_SIZE);

	strcpy(shared_region->cfe_tbl_register.Name, Name);
	shared_region->cfe_tbl_register.TblSize        = TblSize;
	shared_region->cfe_tbl_register.TblOptionFlags = TblOptionFlags;
	/* FIXME: Validation callbacks barely matter, implement them later */

	result = emu_CFE_TBL_Register(spdid);
	if (result == CFE_SUCCESS || result == CFE_TBL_INFO_RECOVERED_TBL) {
		assert(TblSize <= EMU_TBL_BUF_SIZE);

		table_info[shared_region->cfe_tbl_register.TblHandle].size = TblSize;
		*TblHandlePtr = shared_region->cfe_tbl_register.TblHandle;
	}

	return result;
}

CFE_TIME_SysTime_t
CFE_TIME_Add(CFE_TIME_SysTime_t Time1, CFE_TIME_SysTime_t Time2)
{
	shared_region->cfe_time_add.Time1 = Time1;
	shared_region->cfe_time_add.Time2 = Time2;
	emu_CFE_TIME_Add(spdid);
	return shared_region->cfe_time_add.Result;
}

CFE_TIME_Compare_t
CFE_TIME_Compare(CFE_TIME_SysTime_t Time1, CFE_TIME_SysTime_t Time2)
{
	shared_region->cfe_time_compare.Time1 = Time1;
	shared_region->cfe_time_compare.Time2 = Time2;
	emu_CFE_TIME_Compare(spdid);
	return shared_region->cfe_time_compare.Result;
}

CFE_TIME_SysTime_t
CFE_TIME_GetTime(void)
{
	emu_CFE_TIME_GetTime(spdid);
	return shared_region->time;
}

void
CFE_TIME_Print(char *PrintBuffer, CFE_TIME_SysTime_t TimeToPrint)
{
	shared_region->cfe_time_print.TimeToPrint = TimeToPrint;
	emu_CFE_TIME_Print(spdid);
	memcpy(PrintBuffer, shared_region->cfe_time_print.PrintBuffer, CFE_TIME_PRINTED_STRING_SIZE);
}

int32
CFE_TIME_RegisterSynchCallback(CFE_TIME_SynchCallbackPtr_t CallbackFuncPtr)
{
	int i;

	ensure_sync_callbacks_are_setup();

	for (i = 0; i < CFE_TIME_MAX_NUM_SYNCH_FUNCS; i++) {
		if (!sync_callbacks[i]) {
			sync_callbacks[i] = CallbackFuncPtr;
			return CFE_SUCCESS;
		}
	}
	return CFE_TIME_TOO_MANY_SYNCH_CALLBACKS;
}

int32
CFE_TIME_UnregisterSynchCallback(CFE_TIME_SynchCallbackPtr_t CallbackFuncPtr)
{
	int i;
	for (i = 0; i < CFE_TIME_MAX_NUM_SYNCH_FUNCS; i++) {
		if (sync_callbacks[i] == CallbackFuncPtr) {
			sync_callbacks[i] = NULL;
			return CFE_SUCCESS;
		}
	}
	return CFE_TIME_CALLBACK_NOT_REGISTERED;
}

int32
OS_cp(const char *src, const char *dest)
{
	assert(strlen(src) < EMU_BUF_SIZE);
	assert(strlen(dest) < EMU_BUF_SIZE);
	strcpy(shared_region->os_cp.src, src);
	strcpy(shared_region->os_cp.dest, dest);
	return emu_OS_cp(spdid);
}

int32
OS_creat(const char *path, int32 access)
{
	assert(strlen(path) < EMU_BUF_SIZE);

	strcpy(shared_region->os_creat.path, path);
	shared_region->os_creat.access = access;
	return emu_OS_creat(spdid);
}

int32
OS_FDGetInfo(int32 filedes, OS_FDTableEntry *fd_prop)
{
	int32 result;

	shared_region->os_FDGetInfo.filedes = filedes;
	result                              = emu_OS_FDGetInfo(spdid);
	*fd_prop                            = shared_region->os_FDGetInfo.fd_prop;
	return result;
}

int32
OS_fsBytesFree(const char *name, uint64 *bytes_free)
{
	int32 result;

	assert(strlen(name) < EMU_BUF_SIZE);

	strcpy(shared_region->os_fsBytesFree.name, name);
	result      = emu_OS_fsBytesFree(spdid);
	*bytes_free = shared_region->os_fsBytesFree.bytes_free;

	return result;
}

int32
OS_mkdir(const char *path, uint32 access)
{
	assert(strlen(path) < EMU_BUF_SIZE);

	strcpy(shared_region->os_mkdir.path, path);
	shared_region->os_mkdir.access = access;
	return emu_OS_mkdir(spdid);
}

int32
OS_mv(const char *src, const char *dest)
{
	assert(strlen(src) < EMU_BUF_SIZE);
	assert(strlen(dest) < EMU_BUF_SIZE);

	strcpy(shared_region->os_mv.src, src);
	strcpy(shared_region->os_mv.dest, dest);
	return emu_OS_mv(spdid);
}

int32
OS_open(const char *path, int32 access, uint32 mode)
{
	assert(strlen(path) < EMU_BUF_SIZE);

	strcpy(shared_region->os_open.path, path);
	shared_region->os_open.access = access;
	shared_region->os_open.mode   = mode;

	return emu_OS_open(spdid);
}

os_dirp_t
OS_opendir(const char *path)
{
	assert(strlen(path) < EMU_BUF_SIZE);

	strcpy(shared_region->os_opendir.path, path);
	return emu_OS_opendir(spdid);
}

int32
OS_read(int32 filedes, void *buffer, uint32 nbytes)
{
	int32 result;

	assert(nbytes <= EMU_BUF_SIZE);

	shared_region->os_read.filedes = filedes;
	shared_region->os_read.nbytes  = nbytes;
	result                         = emu_OS_read(spdid);
	memcpy(buffer, shared_region->os_read.buffer, nbytes);
	return result;
}

int32
OS_remove(const char *path)
{
	assert(strlen(path) < EMU_BUF_SIZE);

	strcpy(shared_region->os_remove.path, path);
	return emu_OS_remove(spdid);
}


os_dirent_t buffered_dirent;

os_dirent_t *
OS_readdir(os_dirp_t directory)
{
	shared_region->os_readdir.directory = directory;
	emu_OS_readdir(spdid);
	buffered_dirent = shared_region->os_readdir.dirent;
	return &buffered_dirent;
}

int32
OS_rename(const char *old_filename, const char *new_filename)
{
	assert(strlen(old_filename) < EMU_BUF_SIZE);
	assert(strlen(new_filename) < EMU_BUF_SIZE);

	strcpy(shared_region->os_rename.old_filename, old_filename);
	strcpy(shared_region->os_rename.new_filename, new_filename);
	return emu_OS_rename(spdid);
}

int32
OS_rmdir(const char *path)
{
	assert(strlen(path) < EMU_BUF_SIZE);

	strcpy(shared_region->os_rmdir.path, path);
	return emu_OS_rmdir(spdid);
}

int32
OS_stat(const char *path, os_fstat_t *filestats)
{
	int32 result;

	assert(strlen(path) < EMU_BUF_SIZE);

	strcpy(shared_region->os_stat.path, path);
	result     = emu_OS_stat(spdid);
	*filestats = shared_region->os_stat.filestats;
	return result;
}

int32
OS_write(int32 filedes, void *buffer, uint32 nbytes)
{
	assert(nbytes < EMU_BUF_SIZE);
	shared_region->os_write.filedes = filedes;
	memcpy(shared_region->os_write.buffer, buffer, nbytes);
	shared_region->os_write.nbytes = nbytes;
	return emu_OS_write(spdid);
}

int32
OS_BinSemCreate(uint32 *sem_id, const char *sem_name, uint32 sem_initial_value, uint32 options)
{
	int32 result;

	assert(strlen(sem_name) < EMU_BUF_SIZE);

	strcpy(shared_region->os_semCreate.sem_name, sem_name);
	shared_region->os_semCreate.sem_initial_value = sem_initial_value;
	shared_region->os_semCreate.options           = options;
	result                                        = emu_OS_BinSemCreate(spdid);
	*sem_id                                       = shared_region->os_semCreate.sem_id;
	return result;
}

int32
OS_CountSemCreate(uint32 *sem_id, const char *sem_name, uint32 sem_initial_value, uint32 options)
{
	int32 result;

	assert(strlen(sem_name) < EMU_BUF_SIZE);

	strcpy(shared_region->os_semCreate.sem_name, sem_name);
	shared_region->os_semCreate.sem_initial_value = sem_initial_value;
	shared_region->os_semCreate.options           = options;
	result                                        = emu_OS_CountSemCreate(spdid);
	*sem_id                                       = shared_region->os_semCreate.sem_id;
	return result;
}

int32
OS_MutSemCreate(uint32 *sem_id, const char *sem_name, uint32 options)
{
	int32 result;

	assert(strlen(sem_name) < EMU_BUF_SIZE);

	strcpy(shared_region->os_mutSemCreate.sem_name, sem_name);
	shared_region->os_mutSemCreate.options = options;
	result                                 = emu_OS_MutSemCreate(spdid);
	*sem_id                                = shared_region->os_mutSemCreate.sem_id;

	return result;
}

int32
OS_TaskGetIdByName(uint32 *task_id, const char *task_name)
{
	int32 result;

	assert(strlen(task_name) < EMU_BUF_SIZE);

	strcpy(shared_region->os_taskGetIdByName.task_name, task_name);
	result   = emu_OS_TaskGetIdByName(spdid);
	*task_id = shared_region->os_taskGetIdByName.task_id;
	return result;
}

int32
OS_SymbolLookup(cpuaddr *symbol_address, const char *symbol_name)
{
	int32 result;

	assert(strlen(symbol_name) < EMU_BUF_SIZE);

	strcpy(shared_region->os_symbolLookup.symbol_name, symbol_name);
	result          = emu_OS_SymbolLookup(spdid);
	*symbol_address = shared_region->os_symbolLookup.symbol_address;
	return result;
}

/* Methods that are completly emulated */

void
OS_printf(const char *string, ...)
{
	if (emu_is_printf_enabled()) {
		char    s[OS_BUFFER_SIZE];
		va_list arg_ptr;
		int     ret, len = OS_BUFFER_SIZE;

		va_start(arg_ptr, string);
		ret = vsnprintf(s, len, string, arg_ptr);
		va_end(arg_ptr);
		cos_llprint(s, ret);
	}
}

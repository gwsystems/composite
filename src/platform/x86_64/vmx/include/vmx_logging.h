#pragma once

#include <assert.h>
#include <chal.h>
#include <vtxprintf.h>

#define VMX_LOG_ERR      1
#define VMX_LOG_WARNING  2
#define VMX_LOG_NOTICE   3
#define VMX_LOG_INFO     4
#define VMX_LOG_DEBUG    5

#define VMX_LOG_GLOBAL_LEVEL VMX_LOG_NOTICE

static inline int
vmx_log_can_log(u32_t level)
{
	return !(level > VMX_LOG_GLOBAL_LEVEL);
}

#define vmx_assert(x) assert(x)

#define VMX_LOG(level, fmt, args...) { if (unlikely(vmx_log_can_log(VMX_LOG_ ## level))) printk("[VMX_LOG_" #level "]" ": "fmt, ## args); }

#define VMX_DEBUG(fmt, args...) VMX_LOG(DEBUG, fmt, ## args)
#define VMX_INFO(fmt, args...) VMX_LOG(INFO, fmt, ## args)
#define VMX_NOTICE(fmt, args...) VMX_LOG(NOTICE, fmt, ## args)
#define VMX_WARN(fmt, args...) VMX_LOG(WARNING, fmt, ## args)
#define VMX_ERROR(fmt, args...) { VMX_LOG(ERR, fmt, ## args); vmx_assert(0);}

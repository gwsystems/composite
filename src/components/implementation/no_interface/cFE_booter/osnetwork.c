#include "cFE_util.h"

#include "gen/osapi.h"
#include "gen/common_types.h"

/*
** Networking API
*/
int32 OS_SocketOpen(uint32 *sock_id, OS_SocketDomain_t Domain, OS_SocketType_t Type)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_SocketClose(uint32 sock_id)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_SocketBind(uint32 sock_id, const OS_SockAddr_t *Addr)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_SocketConnect(uint32 sock_id, const OS_SockAddr_t *Addr, int32 timeout)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_SocketAccept(uint32 sock_id, uint32 *connsock_id, OS_SockAddr_t *Addr, int32 timeout)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_SocketRecvFrom(uint32 sock_id, void *buffer, uint32 buflen, OS_SockAddr_t *RemoteAddr, int32 timeout)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_SocketSendTo(uint32 sock_id, const void *buffer, uint32 buflen, const OS_SockAddr_t *RemoteAddr)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_SocketGetIdByName (uint32 *sock_id, const char *sock_name)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_SocketGetInfo (uint32 sock_id, OS_socket_prop_t *sock_prop)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_SocketAddrInit(OS_SockAddr_t *Addr, OS_SocketDomain_t Domain)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_SocketAddrToString(char *buffer, uint32 buflen, const OS_SockAddr_t *Addr)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_SocketAddrFromString(OS_SockAddr_t *Addr, const char *string)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_SocketAddrGetPort(uint16 *PortNum, const OS_SockAddr_t *Addr)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_SocketAddrSetPort(OS_SockAddr_t *Addr, uint16 PortNum)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** OS_NetworkGetID is currently [[deprecated]] as its behavior is
** unknown and not consistent across operating systems.
*/
int32 OS_NetworkGetID(void)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_NetworkGetHostName(char *host_name, uint32 name_len)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

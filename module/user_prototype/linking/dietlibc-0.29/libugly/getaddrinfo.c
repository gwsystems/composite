#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <net/if.h>
#include <arpa/inet.h>
#include "dietfeatures.h"

#ifdef WANT_PLUGPLAY_DNS
extern int __dns_plugplay_interface;
#endif

/* XXX TODO FIXME */

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
  struct addrinfo **tmp;
  int family;
  tmp=res; *res=0;
  if (hints) {
    if (hints->ai_family && hints->ai_family != PF_INET6 && hints->ai_family != PF_INET) return EAI_FAMILY;
    if (hints->ai_socktype && hints->ai_socktype != SOCK_STREAM && hints->ai_socktype != SOCK_DGRAM) return EAI_SOCKTYPE;
  }
  for (family=PF_INET6; ; family=PF_INET) {
    if (!hints || hints->ai_family==family || hints->ai_family==AF_UNSPEC) {	/* IPv6 addresses are OK */
      struct hostent h;
      struct hostent *H;
      int herrno=0;
      char buf[4096];
      int lookupok=0;
      char* interface;
      h.h_addr_list=(char**)buf+16;
      if (node) {
	if ((interface=strchr(node,'%'))) ++interface;
	if (inet_pton(family,node,buf)>0) {
	  h.h_name=(char*)node;
	  h.h_addr_list[0]=buf;
	  lookupok=1;
	} else if ((!hints || !(hints->ai_flags&AI_NUMERICHOST)) &&
		   !gethostbyname2_r(node,family,&h,buf,4096,&H,&herrno)) {
	  lookupok=1;
	} else {
	  if (herrno==TRY_AGAIN) { freeaddrinfo(*res); return EAI_AGAIN; }
	}
      } else {
	h.h_name=0;
	h.h_addr_list[0]=buf;
	interface=0;
	memset(buf,0,16);
	if (!hints || !(hints->ai_flags&AI_PASSIVE)) {
	  if (family==AF_INET) {
	    buf[0]=127; buf[3]=1;
	  } else
	    buf[15]=1;
	}
	lookupok=1;
      }
      if (lookupok) {
	struct ai_v6 {
	  struct addrinfo ai;
	  union {
	    struct sockaddr_in6 ip6;
	    struct sockaddr_in ip4;
	  } ip;
	  char name[1];
	} *foo;
	unsigned short port;
	int len=sizeof(struct ai_v6)+(h.h_name?strlen(h.h_name):0);
	if (!(foo=malloc(len))) goto error;
	foo->ai.ai_next=0;
	foo->ai.ai_socktype=SOCK_STREAM;
	foo->ai.ai_protocol=IPPROTO_TCP;
	foo->ai.ai_addrlen=family==PF_INET6?sizeof(struct sockaddr_in6):sizeof(struct sockaddr_in);
	foo->ai.ai_addr=(struct sockaddr*)&foo->ip;
	if (family==PF_INET6) {
	  memset(&foo->ip,0,sizeof(foo->ip));
	  memmove(&foo->ip.ip6.sin6_addr,h.h_addr_list[0],16);
	  if (interface) foo->ip.ip6.sin6_scope_id=if_nametoindex(interface);
	} else {
	  memmove(&foo->ip.ip4.sin_addr,h.h_addr_list[0],4);
	}
	foo->ip.ip6.sin6_family=foo->ai.ai_family=family;
#ifdef WANT_PLUGPLAY_DNS
	if (family==AF_INET6)
	  foo->ip.ip6.sin6_scope_id=__dns_plugplay_interface;
#endif
	if (h.h_name) {
	  foo->ai.ai_canonname=foo->name;
	  memmove(foo->name,h.h_name,strlen(h.h_name)+1);
	} else
	  foo->ai.ai_canonname=0;
	if (!hints || hints->ai_socktype!=SOCK_DGRAM) {	/* TCP is OK */
	  char *x;
	  port=htons(strtol(service?service:"0",&x,0));
	  if (*x) {	/* service is not numeric :-( */
	    struct servent* se;
	    if ((se=getservbyname(service,"tcp"))) {	/* found a service. */
	      port=se->s_port;
  blah1:
	      if (family==PF_INET6)
		foo->ip.ip6.sin6_port=port;
	      else
		foo->ip.ip4.sin_port=port;
	      if (!*tmp) *tmp=&(foo->ai); else (*tmp)->ai_next=&(foo->ai);
	      if (!(foo=malloc(len))) goto error;
	      memmove(foo,*tmp,len);
	      tmp=&(*tmp)->ai_next;
	      foo->ai.ai_addr=(struct sockaddr*)&foo->ip;
	      if (foo->ai.ai_canonname)
		foo->ai.ai_canonname=foo->name;
	    } else {
	      freeaddrinfo(*res);
	      return EAI_SERVICE;
	    }
	  } else goto blah1;
	}
	foo->ai.ai_socktype=SOCK_DGRAM;
	foo->ai.ai_protocol=IPPROTO_UDP;
	if (!hints || hints->ai_socktype!=SOCK_STREAM) {	/* UDP is OK */
	  char *x;
	  port=htons(strtol(service?service:"0",&x,0));
	  if (*x) {	/* service is not numeric :-( */
	    struct servent* se;
	    if ((se=getservbyname(service,"udp"))) {	/* found a service. */
	      port=se->s_port;
blah2:
	      if (family==PF_INET6)
		foo->ip.ip6.sin6_port=port;
	      else
		foo->ip.ip4.sin_port=port;
	      if (!*tmp) *tmp=&(foo->ai); else (*tmp)->ai_next=&(foo->ai);
	      if (!(foo=malloc(len))) goto error;
	      memmove(foo,*tmp,len);
	      tmp=&(*tmp)->ai_next;
	      foo->ai.ai_addr=(struct sockaddr*)&foo->ip;
	      foo->ai.ai_canonname=foo->name;
	    } else {
	      freeaddrinfo(*res);
	      return EAI_SERVICE;
	    }
	  } else goto blah2;
	}
	free(foo);
      }
    }
    if (family==PF_INET) break;
  }
  if (*res==0) return EAI_NONAME; /* kludge kludge... */
  return 0;
error:
  freeaddrinfo(*res);
  return EAI_MEMORY;
}

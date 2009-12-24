#define _BSD_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/poll.h>
#include <stdio.h>
#include <strings.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <fcntl.h>

char myhostname[100];
int namelen;
struct sockaddr* peer;
socklen_t sl;
int s6,s4;
char ifname[10];

struct sockaddr_in mysa4;
struct sockaddr_in6 mysa6;

static void handle(int s,char* buf,int len,int interface) {
  int q;
  char* obuf=buf;
  char* after;
  int olen=len;
  if (len<8*2) return;			/* too short */
  buf[len]=0;
  if ((buf[2]&0xf8) != 0) return;		/* not query */
  q=(((unsigned int)(buf[4])) << 8) | buf[5];
  if (q!=1) return;			/* cannot handle more than 1 query */
  if (buf[6] || buf[7]) return;		/* answer count must be 0 */
  if (buf[8] || buf[9]) return;		/* name server count must be 0 */
  if (buf[10] || buf[11]) return;	/* additional record count must be 0 */
  buf+=12; len-=12;
  if (buf[0]==namelen && !strncasecmp(buf+1,myhostname,namelen)) {
    unsigned int type;
    int slen;
    buf+=namelen+1;
    if (!*buf)
      ++buf;
    else if (!strcmp(buf,"\x05local"))
      buf+=7;
    else
      return;
//    if (((unsigned long)buf)&1) ++buf;
    if (buf[0] || buf[2]) return;	/* all supported types and classes fit in 8 bit */
    if (buf[3]!=1) return;		/* we only support IN queries */
    type=(unsigned char)buf[1];
    slen=buf-obuf+4;
    obuf[2]|=0x80; 	/* it's answer; we don't support recursion */
    if (type==1 || type==255) {		/* A or ANY, we can do that */
      struct ifreq ifr;
      static int v4sock=-1;
      ++obuf[7];			/* one answer */
      memcpy(obuf+slen,"\xc0\x0c" /* goofy compression */
	           "\x00\x01" /* A */
		   "\x00\x01" /* IN */
		   "\x00\x00\x02\x30" /* ttl; 0x230, about 9.3 minutes */
		   "\x00\x04" /* 4 bytes payload */
		   ,12);
      /* now put in our address */
      /* OK, so we know the interface.  Time to find out our IP on that
       * interface.  That is done via
       *   ioctl(somesock,SIOCGIFADDR,struct ifreq)
       * Unfortunately, we need to put the interface _name_ in that
       * struct, not the index.  So we must first call
       *   ioctl(somesock,SIOCGIFINDEX,struct ifreq) */
      if (v4sock==-1) v4sock=s4;
      if (v4sock==-1) v4sock=socket(AF_INET,SOCK_DGRAM,0);
      if (v4sock==-1) return;
      ifr.ifr_ifindex=interface;
      if (ioctl(v4sock,SIOCGIFNAME,&ifr)==-1) return;
      ifr.ifr_addr.sa_family=AF_INET;
      if (ioctl(v4sock,SIOCGIFADDR,&ifr)==-1) return;	/* can't happen */
      memcpy(obuf+slen+12,&((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr,4);
      slen+=4+12;
    }
    if (type==28 || type==255) {	/* AAAA or ANY */
      if (!IN6_IS_ADDR_UNSPECIFIED(mysa6.sin6_addr.s6_addr32)) {
	memcpy(obuf+slen,"\xc0\x0c" /* goofy DNS compression */
			"\x00\x1c" /* AAAA */
			"\x00\x01" /* IN */
			"\x00\x00\x02\x30" /* ttl */
			"\x00\x10" /* 16 bytes payload */
			,12);
	memcpy(obuf+slen+12,&mysa6.sin6_addr,16);
	slen+=28;
	++obuf[7];
      }
    }
    if (obuf[7])
      sendto(s,obuf,slen,0,peer,sl);
  }
}

struct sockaddr_in sa4;
struct sockaddr_in6 sa6;
struct pollfd pfd[2];

struct msghdr mh;
struct iovec iv;
char abuf[100];
#define PKGSIZE 1500
char buf[PKGSIZE+1];

static int scan_fromhex(unsigned char c) {
  if (c>='0' && c<='9')
    return c-'0';
  else if (c>='A' && c<='F')
    return c-'A'+10;
  else if (c>='a' && c<='f')
    return c-'a'+10;
  return -1;
}

static void getip(int interface) {
  int fd;
  struct cmsghdr* x;
  memset(&mysa4,0,sizeof(mysa4));
  memset(&mysa6,0,sizeof(mysa6));
  for (x=CMSG_FIRSTHDR(&mh); x; x=CMSG_NXTHDR(&mh,x))
    if (x->cmsg_level==SOL_IP && x->cmsg_type==IP_PKTINFO)
      mysa4.sin_addr=((struct in_pktinfo*)(CMSG_DATA(x)))->ipi_spec_dst;

  fd=open("/proc/net/if_inet6",O_RDONLY);
  if (fd!=-1) {
    char buf[1024];	/* increase as necessary */
    int i,j,len;
    len=read(fd,buf,sizeof buf);
    if (len>0) {
      int ok;
      char* c=buf;
      char* max=buf+len;
      ok=0;
      /* "fec000000000000102c09ffffe53fc52 01 40 40 00     eth0" */
      while (c<max) {
	int a,b;
	for (i=0; i<16; ++i) {
	  a=scan_fromhex(c[i*2]);
	  b=scan_fromhex(c[i*2+1]);
	  if (a<0 || b<0) goto kaputt;
	  mysa6.sin6_addr.s6_addr[i]=(a<<4)+b;
	}
	ok=1;
	a=scan_fromhex(c[33]);
	b=scan_fromhex(c[34]);
	c+=32;
	if (a<0 || b<0) goto kaputt;
	if ((a<<4)+b == interface) {
	  ok=1;
	  goto kaputt;
	}
	while (c<max && *c!='\n') ++c;
	++c;
      }
kaputt:
      if (!ok) memset(&mysa6,0,sizeof(mysa6));
    }
    close(fd);
  }
}

static int v4if() {
  struct cmsghdr* x;
  for (x=CMSG_FIRSTHDR(&mh); x; x=CMSG_NXTHDR(&mh,x))
    if (x->cmsg_level==SOL_IP && x->cmsg_type==IP_PKTINFO)
      return ((struct in_pktinfo*)(CMSG_DATA(x)))->ipi_ifindex;
  return 0;
}

static void recv4() {
  int len;
  int interface;

  mh.msg_name=&sa4;
  mh.msg_namelen=sizeof(sa4);
  if ((len=recvmsg(s4,&mh,0))==-1) {
    perror("recvmsg");
    exit(3);
  }
  peer=(struct sockaddr*)&sa4;
  sl=sizeof(sa4);

  interface=v4if();
  getip(interface);

  handle(s4,buf,len,interface);
}

static void recv6() {
  int len,interface;

  mh.msg_name=&sa6;
  mh.msg_namelen=sizeof(sa6);
  if ((len=recvmsg(s6,&mh,0))==-1) {
    perror("recvmsg");
    exit(3);
  }
  peer=(struct sockaddr*)&sa6;
  sl=sizeof(sa6);

  if (IN6_IS_ADDR_V4MAPPED(sa6.sin6_addr.s6_addr))
    interface=v4if();
  else
    interface=sa6.sin6_scope_id;

  getip(interface);

  handle(s6,buf,len,interface);
}

int main() {
  mh.msg_name=&sa4;
  mh.msg_namelen=sizeof(sa4);
  mh.msg_iov=&iv;
  mh.msg_iovlen=1;
  iv.iov_base=buf;
  iv.iov_len=PKGSIZE;
  mh.msg_control=abuf;
  mh.msg_controllen=sizeof(abuf);

  if (gethostname(myhostname,64)==-1) {
    perror("gethostname");
    return 1;
  }
  namelen=strlen(myhostname);
  s6=socket(PF_INET6,SOCK_DGRAM,IPPROTO_UDP);
  s4=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
  if (s4==-1 && s6==-1) {
    perror("socket");
    return 2;
  }
  if (s6!=-1) {
    memset(&sa6,0,sizeof(sa6));
    sa6.sin6_family=PF_INET6;
    sa6.sin6_port=htons(5353);
    if (bind(s6,(struct sockaddr*)&sa6,sizeof(struct sockaddr_in6))==-1) {
      perror("bind IPv6");
      close(s6);
      s6=-1;
    }
  }
  if (s4!=-1) {
    memset(&sa4,0,sizeof(sa4));
    sa4.sin_family=PF_INET;
    sa4.sin_port=htons(5353);
    if (bind(s4,(struct sockaddr*)&sa4,sizeof(struct sockaddr_in))==-1) {
      if (errno!=EADDRINUSE || s6==-1)
	perror("bind IPv4");
      close(s4);
      s4=-1;
    }
  }
  if (s4==-1 && s6==-1) return 2;

  {
    int val=255;
    int one=1;
    if (s6!=-1) {
      struct ipv6_mreq opt;
      setsockopt(s6,IPPROTO_IPV6,IPV6_UNICAST_HOPS,&val,sizeof(val));
      setsockopt(s6,IPPROTO_IPV6,IPV6_MULTICAST_LOOP,&one,sizeof(one));
      memcpy(&opt.ipv6mr_multiaddr,"\xff\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfb",16);
      opt.ipv6mr_interface=0;
      setsockopt(s6,IPPROTO_IPV6,IPV6_ADD_MEMBERSHIP,&opt,sizeof opt);
//      setsockopt(s6,IPPROTO_IPV6,IPV6_PKTINFO,&one,sizeof one);
    }
    {
      struct ip_mreq opt;
      int s=(s4==-1?s6:s4);
      setsockopt(s,SOL_IP,IP_TTL,&val,sizeof(val));
      memcpy(&opt.imr_multiaddr.s_addr,"\xe0\x00\x00\xfb",4);
      opt.imr_interface.s_addr=0;
      setsockopt(s,IPPROTO_IP,IP_ADD_MEMBERSHIP,&opt,sizeof(opt));
      setsockopt(s,SOL_IP,IP_PKTINFO,&one,sizeof one);
    }
  }

  for (;;) {
    /* 1500 is the MTU for UDP, I figure we won't longer packets */
    /* add 1 to be able to add \0 */
    int len;
    int interface=0;
    if (s4!=-1 && s6!=-1) {
      if (s4!=-1) {
	recv4();
#if 0
	if ((len=recvmsg(s4,&mh,0))==-1) {
	  perror("recvmsg");
	  return 3;
	}
	peer=(struct sockaddr*)&sa4;
	sl=sizeof(sa4);

	for (x=CMSG_FIRSTHDR(&mh); x; x=CMSG_NXTHDR(&mh,x))
	  if (x->cmsg_level==SOL_IP && x->cmsg_type==IP_PKTINFO) {
	    struct in_pktinfo* y=(struct in_pktinfo*)(CMSG_DATA(x));
	    interface=y->ipi_ifindex;
	    break;
	  }

	handle(s4,buf,len,interface);
#endif
      } else {
	recv6();
#if 0
	sl=sizeof(sa6);
	if ((len=recvfrom(s6,buf,PKGSIZE,0,(struct sockaddr*)&sa6,&sl))==-1) {
	  perror("recvfrom");
	  return 3;
	}
	peer=(struct sockaddr*)&sa6;

	handle(s6,buf,len,sa6.sin6_scope_id);
#endif
      }
    } else {
      pfd[0].fd=s4; pfd[0].events=POLLIN;
      pfd[1].fd=s6; pfd[1].events=POLLIN;
      switch (poll(pfd,2,5*1000)) {
      case -1:
	if (errno==EINTR) continue;
	perror("poll");
	return 1;
      case 0:
	continue;
      }
      if (pfd[0].revents & POLLIN) {
	recv4();
#if 0
	if ((len=recvmsg(s4,&mh,0))==-1) {
	  perror("recvmsg");
	  return 3;
	}
	peer=(struct sockaddr*)&sa4;
	sl=sizeof(sa4);

	for (x=CMSG_FIRSTHDR(&mh); x; x=CMSG_NXTHDR(&mh,x))
	  if (x->cmsg_level==SOL_IP && x->cmsg_type==IP_PKTINFO) {
	    struct in_pktinfo* y=(struct in_pktinfo*)(CMSG_DATA(x));
	    interface=y->ipi_ifindex;
	    break;
	  }

	handle(s4,buf,len,interface);
#endif
      }
      if (pfd[1].revents & POLLIN) {
	recv6();
#if 0
	sl=sizeof(sa6);
	if ((len=recvfrom(s6,buf,sizeof(buf),0,(struct sockaddr*)&sa6,&sl))==-1) {
	  perror("recvfrom");
	  return 3;
	}
	peer=(struct sockaddr*)&sa6;
	handle(s6,buf,len,sa6.sin6_scope_id);
#endif
      }
    }
  }
  return 0;
}

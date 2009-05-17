#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#include <cos_alloc.h>

#define MEM_LIBC_MALLOC 1
#define MEM_USE_POOLS 0

#define NO_SYS 1
#define MEM_ALIGNMENT 4
#define MEM_SIZE 0 //64000
/* #define MEMP_OVERFLOW_CHECK 1 */
/* #define MEMP_SANITY_CHECK 1 */
#define MEMP_NUM_PBUF (4096*4)
#define MEMP_NUM_UDP_PCB 512
//#define MEMP_NUM_TCP_PCB 512
#define MEMP_NUM_TCP_PCB 8192 	/* need a fair amount of these due to timed wait on close  */
#define MEMP_NUM_TCP_PCB_LISTEN 128
#define LWIP_ARP 0
#define IP_REASSEMBLY 0
#define IP_FRAG 0

/*#define SYS_LIGHTWEIGHT_PROT           1 */

/* TCP */
#define LWIP_TCP                1
#define TCP_TTL                 255

/* Controls if TCP should queue segments that arrive out of
   order. Define to 0 if your device is low on memory. */
#define TCP_QUEUE_OOSEQ         1

/* TCP Maximum segment size. */
#define TCP_MSS                 1400 //1024

/* TCP sender buffer space (bytes). */
#define TCP_SND_BUF             TCP_MSS*24//TCP_MSS*6//2048

/* TCP sender buffer space (pbufs). This must be at least = 2 *
   TCP_SND_BUF/TCP_MSS for things to work. */
//#define TCP_SND_QUEUELEN        (4 * TCP_SND_BUF/TCP_MSS)
#define TCP_SND_QUEUELEN        (4 * TCP_SND_BUF/TCP_MSS)

#define MEMP_NUM_TCP_SEG (TCP_SND_QUEUELEN*16)

/* TCP writable space (bytes). This must be less than or equal
   to TCP_SND_BUF. It is the amount of space which must be
   available in the tcp snd_buf for select to return writable */
#define TCP_SNDLOWAT		(TCP_SND_BUF/2)

/* TCP receive window. */
#define TCP_WND                 (8096*2)

/* Maximum number of retransmissions of data segments. */
#define TCP_MAXRTX              12

/* Maximum number of retransmissions of SYN segments. */
#define TCP_SYNMAXRTX           4

#define TCP_LISTEN_BACKLOG      1

#undef LWIP_EVENT_API
#define LWIP_CALLBACK_API 1

//#define LWIP_STATS 0
//#define LWIP_STATS	                1
//#define LWIP_STATS_DISPLAY              1
/* #define IP_STATS                        1 */
/* #define LINK_STATS                      0 */
/* #define ETHARP_STATS                    0 */
/* #define ICMP_STATS                      0 */
/* #define IGMP_STATS                      0 */
/* #define IPFRAG_STATS                    0 //(IP_REASSEMBLY || IP_FRAG) */
/* #define UDP_STATS                       0 //(LWIP_UDP) */
//#define TCP_STATS                       (LWIP_TCP)
/* #define MEM_STATS                       1 */
/* #define MEMP_STATS                      1 */

#define PPP_SUPPORT      0      /* Set > 0 for PPP */

#define ARP_QUEUEING 0
#define LWIP_SOCKET 0
#define LWIP_NETCONN 0
#define LWIP_ICMP   0

#define LWIP_DEBUG  1
#define LWIP_DBG_TYPES_ON  LWIP_DBG_ON
#define LWIP_DBG_MIN_LEVEL 1

/* #define PPP_DEBUG        LWIP_DBG_OFF */
//#define MEM_DEBUG        (LWIP_DBG_ON | LWIP_DBG_TRACE)
//#define MEMP_DEBUG       (LWIP_DBG_ON | LWIP_DBG_TRACE)
//#define PBUF_DEBUG       (LWIP_DBG_ON | LWIP_DBG_TRACE)
/* #define API_LIB_DEBUG    LWIP_DBG_ON */
/* #define API_MSG_DEBUG    LWIP_DBG_ON */
//#define TCPIP_DEBUG      (LWIP_DBG_ON | LWIP_DBG_LEVEL_WARNING)
/* #define NETIF_DEBUG      LWIP_DBG_ON */
/* #define SOCKETS_DEBUG    LWIP_DBG_ON */
/* #define DEMO_DEBUG       LWIP_DBG_ON */
//#daefine IP_DEBUG         (LWIP_DBG_ON | LWIP_DBG_LEVEL_WARNING)
/* #define IP_REASS_DEBUG   LWIP_DBG_ON */
/* #define RAW_DEBUG        LWIP_DBG_ON */
/* #define ICMP_DEBUG       LWIP_DBG_ON */
/* #define UDP_DEBUG        LWIP_DBG_ON */
//#define TCP_DEBUG        (LWIP_DBG_ON  | LWIP_DBG_LEVEL_WARNING)
//#define TCP_INPUT_DEBUG  (LWIP_DBG_ON | LWIP_DBG_LEVEL_WARNING)
//#define TCP_OUTPUT_DEBUG (LWIP_DBG_ON | LWIP_DBG_LEVEL_WARNING)
//#define TCP_RTO_DEBUG    (LWIP_DBG_ON | LWIP_DBG_LEVEL_WARNING)
//#define TCP_CWND_DEBUG   (LWIP_DBG_ON | LWIP_DBG_LEVEL_WARNING)
//#define TCP_WND_DEBUG    (LWIP_DBG_ON | LWIP_DBG_LEVEL_WARNING)
/* #define TCP_FR_DEBUG     LWIP_DBG_ON */
//#define TCP_QLEN_DEBUG   (LWIP_DBG_ON  | LWIP_DBG_LEVEL_WARNING)
//#define TCP_RST_DEBUG    (LWIP_DBG_ON | LWIP_DBG_LEVEL_WARNING)

//#define LWIP_DEBUG 1
//#define LWIP_DBG_TYPES_ON  LWIP_DBG_OFF 
//#define LWIP_DBG_MIN_LEVEL 0
//#define LWIP_DBG_MIN_LEVEL 1

#define PPP_DEBUG        LWIP_DBG_OFF
#define MEM_DEBUG        LWIP_DBG_OFF
#define MEMP_DEBUG       LWIP_DBG_OFF
#define PBUF_DEBUG       LWIP_DBG_OFF
#define API_LIB_DEBUG    LWIP_DBG_OFF
#define API_MSG_DEBUG    LWIP_DBG_OFF
#define TCPIP_DEBUG      LWIP_DBG_OFF
#define NETIF_DEBUG      LWIP_DBG_OFF
#define SOCKETS_DEBUG    LWIP_DBG_OFF
#define DEMO_DEBUG       LWIP_DBG_OFF
#define IP_DEBUG         LWIP_DBG_OFF
#define IP_REASS_DEBUG   LWIP_DBG_OFF
#define RAW_DEBUG        LWIP_DBG_OFF
#define ICMP_DEBUG       LWIP_DBG_OFF
#define UDP_DEBUG        LWIP_DBG_OFF
#define TCP_DEBUG        LWIP_DBG_OFF
#define TCP_INPUT_DEBUG  LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG LWIP_DBG_OFF
#define TCP_RTO_DEBUG    LWIP_DBG_OFF
#define TCP_CWND_DEBUG   LWIP_DBG_OFF
#define TCP_WND_DEBUG    LWIP_DBG_OFF
#define TCP_FR_DEBUG     LWIP_DBG_OFF
#define TCP_QLEN_DEBUG   LWIP_DBG_OFF
#define TCP_RST_DEBUG    LWIP_DBG_OFF 

/*extern unsigned char debug_flags;
  #define LWIP_DBG_TYPES_ON debug_flags*/


#endif	/* LWIPOPTS_H */

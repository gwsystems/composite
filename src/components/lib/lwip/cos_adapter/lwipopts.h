#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#define MEM_LIBC_MALLOC 0
#define MEM_USE_POOLS 0

#define NO_SYS 1
#define MEM_ALIGNMENT 8
#define MEM_SIZE  (12 * 1024)//64000
// #define MEMP_OVERFLOW_CHECK 1
// #define MEMP_SANITY_CHECK 1
#define MEMP_NUM_PBUF (4096*4)

#define LWIP_ETHERNET 1
#define LWIP_ARP 1
#define ETHARP_SUPPORT_STATIC_ENTRIES 1
#define LWIP_IPV4 1

#define MEMP_NUM_UDP_PCB 512

#define MEMP_NUM_TCP_PCB 512 	/* need a fair amount of these due to timed wait on close  */
#define MEMP_NUM_TCP_PCB_LISTEN 128
#define IP_REASSEMBLY 0
#define IP_FRAG 0

#define SYS_LIGHTWEIGHT_PROT           0
//#define LWIP_WND_SCALE                  1

/* TCP */
#define LWIP_TCP                1
#define TCP_TTL                 255

/* Controls if TCP should queue segments that arrive out of
   order. Define to 0 if your device is low on memory. */
#define TCP_QUEUE_OOSEQ         1

/* TCP sender buffer space (pbufs). This must be at least = 2 *
   TCP_SND_BUF/TCP_MSS for things to work. */
#define TCP_SND_QUEUELEN       4096 //(64 * TCP_SND_BUF/TCP_MSS)

#define MEMP_NUM_TCP_SEG 2*4096 //(TCP_SND_QUEUELEN*16)

/* TCP receive window. */
#define TCP_WND                 4096

/* Maximum number of retransmissions of data segments. */
#define TCP_MAXRTX              12

/* Maximum number of retransmissions of SYN segments. */
#define TCP_SYNMAXRTX           4

#define TCP_LISTEN_BACKLOG      1

#undef LWIP_EVENT_API
#define LWIP_CALLBACK_API 1

#define PPP_SUPPORT      0      /* Set > 0 for PPP */

#define ARP_QUEUEING 0
#define LWIP_SOCKET 0
#define LWIP_NETCONN 0
#define LWIP_ICMP   1

#define LWIP_DBG_MIN_LEVEL              LWIP_DBG_LEVEL_ALL
// #define LWIP_DBG_MIN_LEVEL              LWIP_DBG_LEVEL_WARNING
// #define LWIP_DBG_MIN_LEVEL              LWIP_DBG_LEVEL_SERIOUS
// #define LWIP_DBG_MIN_LEVEL              LWIP_DBG_LEVEL_SEVERE

// #define LWIP_DBG_TYPES_ON               (LWIP_DBG_ON|LWIP_DBG_TRACE|LWIP_DBG_STATE|LWIP_DBG_FRESH)

/* Set this to 1 to enable lwip output debug messages, this requires to recompile lwip */
#define LWIP_DEBUG  0

#if LWIP_DEBUG
#define LWIP_DBG_TYPES_ON               LWIP_DBG_ON
#else
#define LWIP_DBG_TYPES_ON               LWIP_DBG_OFF
#endif

#define ETHARP_DEBUG                    LWIP_DBG_ON
#define NETIF_DEBUG                     LWIP_DBG_ON
#define PBUF_DEBUG                      LWIP_DBG_ON
#define API_LIB_DEBUG                   LWIP_DBG_ON
#define API_MSG_DEBUG                   LWIP_DBG_ON
#define SOCKETS_DEBUG                   LWIP_DBG_ON
#define ICMP_DEBUG                      LWIP_DBG_ON
#define IGMP_DEBUG                      LWIP_DBG_ON
#define INET_DEBUG                      LWIP_DBG_ON
#define IP_DEBUG                        LWIP_DBG_ON
#define IP_REASS_DEBUG                  LWIP_DBG_ON
#define RAW_DEBUG                       LWIP_DBG_ON
#define MEM_DEBUG                       LWIP_DBG_ON
#define MEMP_DEBUG                      LWIP_DBG_ON
#define SYS_DEBUG                       LWIP_DBG_ON
#define TCP_DEBUG                       LWIP_DBG_ON
#define TCP_INPUT_DEBUG                 LWIP_DBG_ON
#define TCP_FR_DEBUG                    LWIP_DBG_ON
#define TCP_RTO_DEBUG                   LWIP_DBG_ON
#define TCP_CWND_DEBUG                  LWIP_DBG_ON
#define TCP_WND_DEBUG                   LWIP_DBG_ON
#define TCP_OUTPUT_DEBUG                LWIP_DBG_ON
#define TCP_RST_DEBUG                   LWIP_DBG_ON
#define TCP_QLEN_DEBUG                  LWIP_DBG_ON
#define UDP_DEBUG                       LWIP_DBG_ON
#define TCPIP_DEBUG                     LWIP_DBG_ON
#define PPP_DEBUG                       LWIP_DBG_ON
#define SLIP_DEBUG                      LWIP_DBG_ON
#define DHCP_DEBUG                      LWIP_DBG_ON
#define AUTOIP_DEBUG                    LWIP_DBG_ON
#define SNMP_MSG_DEBUG                  LWIP_DBG_ON
#define SNMP_MIB_DEBUG                  LWIP_DBG_ON
#define DNS_DEBUG                       LWIP_DBG_ON 


#endif	/* LWIPOPTS_H */

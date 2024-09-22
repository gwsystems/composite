/*
 * ndpi_util.h
 *
 * Copyright (C) 2011-16 - ntop.org
 *
 * nDPI is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * nDPI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with nDPI.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * This module contains routines to help setup a simple nDPI program.
 *
 * If you concern about performance or have to integrate nDPI in your
 * application, you could need to reimplement them yourself.
 *
 * WARNING: this API is unstable! Use it at your own risk!
 */
#ifndef __NDPI_UTIL_H__
#define __NDPI_UTIL_H__

struct pcap_pkthdr {
	struct timeval ts;	/* time stamp */
	bpf_u_int32 caplen;	/* length of portion present */
	bpf_u_int32 len;	/* length this packet (off wire) */
};

#define MAX_NUM_READER_THREADS 16
#define IDLE_SCAN_PERIOD 10 /* msec (use TICK_RESOLUTION = 1000) */
#define MAX_IDLE_TIME 30000
#define IDLE_SCAN_BUDGET 1024
#define NUM_ROOTS 512
#define MAX_NDPI_FLOWS 200000000
#define TICK_RESOLUTION 1000
#define MAX_NUM_IP_ADDRESS 5 /* len of ip address array */
#define UPDATED_TREE 1
#define AGGRESSIVE_PERCENT 95.00
#define DIR_SRC 10
#define DIR_DST 20

#define PLEN_MAX         1504
#define PLEN_BIN_LEN     32
#define PLEN_NUM_BINS    48 /* 47*32 = 1504 */
#define MAX_NUM_BIN_PKTS 256
#define MAX_NUM_PKTS               10
#define MAX_BYTE_COUNT_ARRAY_LENGTH 256


enum info_type {
    INFO_INVALID = 0,
    INFO_GENERIC,
    INFO_KERBEROS,
    INFO_SOFTETHER,
    INFO_TIVOCONNECT,
    INFO_FTP_IMAP_POP_SMTP,
    INFO_NATPMP,
    INFO_RTP
};

struct ndpi_entropy {
  // Entropy fields
  u_int16_t src2dst_pkt_len[MAX_NUM_PKTS];                     /*!< array of packet appdata lengths */
  pkt_timeval src2dst_pkt_time[MAX_NUM_PKTS];               /*!< array of arrival times          */
  u_int16_t dst2src_pkt_len[MAX_NUM_PKTS];                     /*!< array of packet appdata lengths */
  pkt_timeval dst2src_pkt_time[MAX_NUM_PKTS];               /*!< array of arrival times          */
  pkt_timeval src2dst_start;                                /*!< first packet arrival time       */
  pkt_timeval dst2src_start;                                /*!< first packet arrival time       */
  u_int32_t src2dst_opackets;                                  /*!< non-zero packet counts          */
  u_int32_t dst2src_opackets;                                  /*!< non-zero packet counts          */
  u_int16_t src2dst_pkt_count;                                 /*!< packet counts                   */
  u_int16_t dst2src_pkt_count;                                 /*!< packet counts                   */
  u_int32_t src2dst_l4_bytes;                                  /*!< packet counts                   */
  u_int32_t dst2src_l4_bytes;                                  /*!< packet counts                   */
  u_int32_t src2dst_byte_count[MAX_BYTE_COUNT_ARRAY_LENGTH];   /*!< number of occurences of each byte   */
  u_int32_t dst2src_byte_count[MAX_BYTE_COUNT_ARRAY_LENGTH];   /*!< number of occurences of each byte   */
  u_int32_t src2dst_num_bytes;
  u_int32_t dst2src_num_bytes;
  double src2dst_bd_mean;
  double src2dst_bd_variance;
  double dst2src_bd_mean;
  double dst2src_bd_variance;
  float score;
};

typedef struct ndpi_flow_info {
  u_int32_t flow_id;
  u_int32_t hashval;
  uint64_t last_seen;
  u_int32_t src_ip; /* network order */
  u_int32_t dst_ip; /* network order */
  struct ndpi_in6_addr src_ip6; /* network order */
  struct ndpi_in6_addr dst_ip6; /* network order */
  u_int16_t src_port; /* network order */
  u_int16_t dst_port; /* network order */
  u_int8_t detection_completed, protocol, bidirectional, check_extra_packets;
  u_int16_t vlan_id;
  ndpi_packet_tunnel tunnel_type;
  struct ndpi_flow_struct *ndpi_flow;
  char src_name[INET6_ADDRSTRLEN], dst_name[INET6_ADDRSTRLEN];
  u_int8_t ip_version;
  u_int32_t cwr_count, src2dst_cwr_count, dst2src_cwr_count;
  u_int32_t ece_count, src2dst_ece_count, dst2src_ece_count;
  u_int32_t urg_count, src2dst_urg_count, dst2src_urg_count;
  u_int32_t ack_count, src2dst_ack_count, dst2src_ack_count;
  u_int32_t psh_count, src2dst_psh_count, dst2src_psh_count;
  u_int32_t syn_count, src2dst_syn_count, dst2src_syn_count;
  u_int32_t fin_count, src2dst_fin_count, dst2src_fin_count;
  u_int32_t rst_count, src2dst_rst_count, dst2src_rst_count;
  u_int32_t c_to_s_init_win, s_to_c_init_win;
  u_int64_t first_seen_ms, last_seen_ms;
  u_int64_t src2dst_bytes, dst2src_bytes;
  u_int64_t src2dst_goodput_bytes, dst2src_goodput_bytes;
  u_int32_t src2dst_packets, dst2src_packets;
  u_int32_t has_human_readeable_strings;
  char human_readeable_string_buffer[32];
  char *risk_str;

  // result only, not used for flow identification
  ndpi_protocol detected_protocol;
  ndpi_confidence_t confidence;
  u_int16_t num_dissector_calls;

  // Flow data analysis
  pkt_timeval src2dst_last_pkt_time, dst2src_last_pkt_time, flow_last_pkt_time;
  struct ndpi_analyze_struct *iat_c_to_s, *iat_s_to_c, *iat_flow,
    *pktlen_c_to_s, *pktlen_s_to_c;

  enum info_type info_type;
  union {
    char info[256];
    struct {
      unsigned char auth_failed;
      char username[127];
      char password[128];
    } ftp_imap_pop_smtp;
    struct {
      char domain[85];
      char hostname[85];
      char username[86];
    } kerberos;
    struct {
      char ip[16];
      char port[6];
      char hostname[48];
      char fqdn[48];
    } softether;
    struct {
      char identity_uuid[36];
      char machine[48];
      char platform[32];
      char services[48];
    } tivoconnect;
    struct  {
      uint16_t result_code;
      uint16_t internal_port;
      uint16_t external_port;
      char ip[16];
    } natpmp;
  };

  ndpi_serializer ndpi_flow_serializer;

  char flow_extra_info[16];
  char host_server_name[80]; /* Hostname/SNI */
  char *bittorent_hash;
  char *dhcp_fingerprint;
  char *dhcp_class_ident;
  ndpi_risk risk;
  
  struct {
    u_int16_t ssl_version;
    char server_info[64],
      client_hassh[33], server_hassh[33], *server_names,
      *advertised_alpns, *negotiated_alpn, *tls_supported_versions,
      *tls_issuerDN, *tls_subjectDN,
      ja3_client[33], ja3_server[33],
      sha1_cert_fingerprint[20];
    u_int8_t sha1_cert_fingerprint_set;
    struct tls_heuristics browser_heuristics;
    
    struct {
      u_int16_t cipher_suite;
      char *esni;
    } encrypted_sni;    

    time_t notBefore, notAfter;
    u_int16_t server_cipher;
    ndpi_cipher_weakness client_unsafe_cipher, server_unsafe_cipher;
  } ssh_tls;

  struct {
    enum ndpi_rtp_stream_type stream_type;
  } rtp;

  struct {
    char url[256], request_content_type[64], content_type[64], user_agent[256], server[128];
    u_int response_status_code;
  } http;

  struct {
    char *username, *password;
  } telnet;

  void *src_id, *dst_id;

  struct ndpi_entropy *entropy;
  struct ndpi_entropy *last_entropy;

  /* Payload lenght bins */
#ifdef DIRECTION_BINS
  struct ndpi_bin payload_len_bin_src2dst, payload_len_bin_dst2src;
#else
  struct ndpi_bin payload_len_bin;
#endif

  /* Flow payload */
  u_int16_t flow_payload_len;
  char *flow_payload;  
} ndpi_flow_info_t;

// flow statistics info
typedef struct ndpi_stats {
        u_int32_t guessed_flow_protocols;
        u_int64_t raw_packet_count;
        u_int64_t ip_packet_count;
        u_int64_t total_wire_bytes, total_ip_bytes, total_discarded_bytes;
        u_int64_t protocol_counter[NDPI_MAX_SUPPORTED_PROTOCOLS + NDPI_MAX_NUM_CUSTOM_PROTOCOLS + 1];
        u_int64_t protocol_counter_bytes[NDPI_MAX_SUPPORTED_PROTOCOLS + NDPI_MAX_NUM_CUSTOM_PROTOCOLS + 1];
        u_int32_t protocol_flows[NDPI_MAX_SUPPORTED_PROTOCOLS + NDPI_MAX_NUM_CUSTOM_PROTOCOLS + 1];
        u_int32_t ndpi_flow_count;
        u_int64_t tcp_count, udp_count;
        u_int64_t mpls_count, pppoe_count, vlan_count, fragmented_count;
        u_int64_t packet_len[6];
        u_int16_t max_packet_len;
	u_int32_t flow_count[3];
	u_int64_t dpi_packet_count[3];
} ndpi_stats_t;

// flow preferences
typedef struct ndpi_workflow_prefs {
  u_int8_t decode_tunnels;
  u_int8_t quiet_mode;
  u_int8_t ignore_vlanid;
  u_int32_t num_roots;
  u_int32_t max_ndpi_flows;
} ndpi_workflow_prefs_t;

struct ndpi_workflow;

/** workflow, flow, user data */
typedef void (*ndpi_workflow_callback_ptr)(struct ndpi_workflow *, struct ndpi_flow_info *, void *);

// workflow main structure
typedef struct ndpi_workflow {
        u_int64_t last_time;

        struct ndpi_workflow_prefs prefs;
        struct ndpi_stats stats;

        ndpi_workflow_callback_ptr __flow_detected_callback;
        void *__flow_detected_udata;
        ndpi_workflow_callback_ptr __flow_giveup_callback;
        void *__flow_giveup_udata;

        /* allocated by prefs */
        void **ndpi_flows_root;
	u_int32_t num_allocated_flows;
        struct ndpi_detection_module_struct *ndpi_struct;
	ndpi_serialization_format ndpi_serialization_format;
} ndpi_workflow_t;

/* TODO: remove wrappers parameters and use ndpi global, when their initialization will be fixed... */
struct ndpi_workflow *
ndpi_workflow_init(const struct ndpi_workflow_prefs *prefs);

/* workflow main free function */
void
ndpi_workflow_free(struct ndpi_workflow *workflow);

/** Free flow_info ndpi support structures but not the flow_info itself
 *
 *  TODO remove! Half freeing things is bad!
 */
void
ndpi_free_flow_info_half(struct ndpi_flow_info *flow);

/* Process a packet and update the workflow  */
struct ndpi_proto ndpi_workflow_process_packet(struct ndpi_workflow * workflow,
					       const struct pcap_pkthdr *header,
					       const u_char *packet,
					       ndpi_risk *flow_risk);


/* flow callbacks for complete detected flow
   (ndpi_flow_info will be freed right after) */
static inline void
ndpi_workflow_set_flow_detected_callback(struct ndpi_workflow *workflow, ndpi_workflow_callback_ptr callback,
                                         void *udata) {
        workflow->__flow_detected_callback = callback;
        workflow->__flow_detected_udata = udata;
}

/* flow callbacks for sufficient detected flow
   (ndpi_flow_info will be freed right after) */
static inline void
ndpi_workflow_set_flow_giveup_callback(struct ndpi_workflow *workflow, ndpi_workflow_callback_ptr callback,
                                       void *udata) {
        workflow->__flow_giveup_callback = callback;
        workflow->__flow_giveup_udata = udata;
}

/* compare two nodes in workflow */
int
ndpi_workflow_node_cmp(const void *a, const void *b);
void
process_ndpi_collected_info(struct ndpi_workflow *workflow, struct ndpi_flow_info *flow);
u_int32_t
ethernet_crc32(const void *data, size_t n_bytes);
void
ndpi_flow_info_freer(void *node);
#endif
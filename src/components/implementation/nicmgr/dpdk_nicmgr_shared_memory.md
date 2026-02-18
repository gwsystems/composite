# DPDK/Nicmgr/Netio Shared Memory Architecture

## Overview

This document explains how packet data flows through the Composite VMM networking stack using DPDK and shared memory. Understanding this architecture is critical for debugging network issues, especially RX path problems.

## Components

### 1. DPDK (Data Plane Development Kit)
- **Location**: `src/components/implementation/nicmgr/dpdk/`
- **Purpose**: Direct userspace access to physical NIC using poll-mode drivers
- **Key Files**:
  - `init.c`: Main RX/TX polling loops
  - `nicmgr.c`: Session management and packet routing

### 2. Nicmgr (Network Interface Card Manager)
- **Purpose**: Routes packets between DPDK and application components (e.g., VMM)
- **Responsibilities**:
  - Receives packets from DPDK mbufs
  - Copies packet data into shared memory buffers
  - Routes packets to correct session based on IP/port
  - Manages ring buffers for packet queuing

### 3. Netio (Network I/O Interface)
- **Location**: Component-specific (e.g., `simple_vmm/vmm/devices/vpci/virtio_net_io.c`)
- **Purpose**: Application-side interface to receive/send packets via nicmgr
- **Responsibilities**:
  - Binds to IP address to receive packets
  - Transfers packet buffers from shared memory
  - Processes packets (e.g., injects into guest VM)

## Shared Memory Structure

### netshmem_pkt_buf
```c
struct netshmem_pkt_buf {
    char data[PKT_BUF_SIZE];  // Total buffer size (e.g., 2048 bytes)
};
```

### Buffer Layout
```
+---------------------------+ <- obj (base pointer)
|  NETSHMEM_HEADROOM (256)  | <- Reserved space for metadata
+---------------------------+ <- obj->data + NETSHMEM_HEADROOM
|                           |
|   Actual Packet Data      | <- This is where Ethernet frame starts
|                           |
+---------------------------+
|       Tailroom            |
+---------------------------+
```

**CRITICAL**: The actual packet data must be written at offset `NETSHMEM_HEADROOM` (256 bytes) from the buffer base, NOT at offset 0!

## RX Path (Receiving Packets)

### Step 1: DPDK Receives Packet
**File**: `nicmgr/dpdk/init.c` - `cos_nic_start()`

```c
// Poll for packets from physical NIC
nb_pkts = cos_dev_port_rx_burst(0, 0, rx_packets, MAX_PKT_BURST);

// Process each received packet
process_rx_packets(0, rx_packets, nb_pkts);
```

### Step 2: Parse and Route Packet
**File**: `nicmgr/dpdk/init.c` - `process_rx_packets()`

```c
// Get packet data from DPDK mbuf
pkt = cos_get_packet(rx_pkts[i], &len);
eth = (struct eth_hdr *)pkt;

// Parse EtherType
u16_t eth_type = ntohs(eth->ether_type);

if (eth_type == 0x0806) {
    // ARP packet - no ports
    ip = arp_hdr->arp_data.arp_tip;
    svc_id = 0;
}
else if (eth_type == 0x0800) {
    // IPv4 packet
    iph = (struct ip_hdr *)((char *)eth + sizeof(struct eth_hdr));
    ip = iph->dst_addr;
    
    if (iph->proto == 1) {
        // ICMP - no ports
        svc_id = 0;
    } else {
        // TCP/UDP - parse port
        port = (struct tcp_udp_port *)((char *)eth + sizeof(struct eth_hdr) + iph->ihl * 4);
        svc_id = ntohs(port->dst_port);
    }
}

// Find session for this IP/port combination
session = simple_hash_find(ip, svc_id);

// Enqueue DPDK mbuf pointer into session's ring buffer
buf.pkt = rx_pkts[i];
pkt_ring_buf_enqueue(&(session->pkt_ring_buf), &buf);

// Wake up the waiting thread
sync_sem_give(&session->sem);
```

### Step 3: Copy Packet to Shared Memory
**File**: `nicmgr/dpdk/nicmgr.c` - `nic_netio_rx_packet()`

```c
// Dequeue DPDK mbuf from ring buffer
pkt_ring_buf_dequeue(&session->pkt_ring_buf, &buf);

// Get packet data from DPDK mbuf
char *pkt = cos_get_packet(buf.pkt, &len);

// Allocate shared memory buffer
obj = shm_bm_alloc_net_pkt_buf(session->shemem_info.shm, &objid);

// CRITICAL: Use netshmem_get_data_buf() to get correct offset
char *data_buf = (char *)netshmem_get_data_buf(obj);
memcpy(data_buf, pkt, len);

// Free DPDK mbuf
cos_free_packet(buf.pkt);

// Return shared memory object ID to caller
return objid;
```

### Step 4: Application Receives Packet
**File**: `simple_vmm/vmm/devices/vpci/virtio_net_io.c` - `virtio_rx_task()`

```c
// Wait for packet from nicmgr
rx_pktid = nic_netio_rx_packet(&pkt_len);

// Transfer ownership from nicmgr's shared memory to our mapping
rx_obj = shm_bm_transfer_net_pkt_buf(rx_shmemd, rx_pktid);

// Get packet data pointer (automatically adds NETSHMEM_HEADROOM offset)
u8_t *pkt_data = (u8_t *)netshmem_get_data_buf(rx_obj);

// Process packet (e.g., inject into guest VM)
virtio_net_rcv_one_pkt(pkt_data, pkt_len);

// Free shared memory buffer
shm_bm_free_net_pkt_buf(rx_obj);
```

## TX Path (Sending Packets)

### Step 1: Application Sends Packet
**File**: `simple_vmm/vmm/devices/vpci/virtio_net_io.c` - `virtio_tx_task()`

```c
// Allocate shared memory buffer
tx_obj = shm_bm_alloc_net_pkt_buf(tx_shmemd, &tx_pktid);

// Get data buffer pointer with proper offset
char *data_buf = (char *)netshmem_get_data_buf(tx_obj);

// Copy packet from guest to shared memory
memcpy_fast(data_buf, pkt, plen);

// Send to nicmgr
nic_netio_tx_packet(tx_pktid, 0, plen);
```

### Step 2: Nicmgr Transmits via DPDK
**File**: `nicmgr/dpdk/nicmgr.c` - `nic_netio_tx_packet()`

```c
// Transfer ownership from application's shared memory
obj = shm_bm_transfer_net_pkt_buf(client_sessions[thd].shemem_info.shm, objid);

// Allocate DPDK mbuf
mbuf = cos_allocate_mbuf(g_tx_mp[queue_idx]);

// Attach shared memory buffer to DPDK mbuf (zero-copy)
cos_attach_external_mbuf(mbuf, obj, paddr, PKT_BUF_SIZE, 
                         ext_buf_free_callback_fn, ext_shinfo);

// Transmit via DPDK
cos_dev_port_tx_burst(0, 0, tx_packets, 1);
```

## Key API Functions

### netshmem_get_data_buf()
```c
static inline void * netshmem_get_data_buf(struct netshmem_pkt_buf *pkt_buf)
{
    return (char *)pkt_buf + NETSHMEM_HEADROOM;
}
```
**Purpose**: Returns pointer to actual packet data area (after headroom)

**ALWAYS USE THIS** when reading or writing packet data!

### shm_bm_alloc_net_pkt_buf()
```c
struct netshmem_pkt_buf *shm_bm_alloc_net_pkt_buf(shm_bm_t shm, shm_bm_objid_t *objid);
```
**Purpose**: Allocates a packet buffer from shared memory pool

### shm_bm_transfer_net_pkt_buf()
```c
struct netshmem_pkt_buf *shm_bm_transfer_net_pkt_buf(shm_bm_t shm, shm_bm_objid_t objid);
```
**Purpose**: Transfers buffer ownership between components (maps object ID to local address)

### shm_bm_free_net_pkt_buf()
```c
void shm_bm_free_net_pkt_buf(struct netshmem_pkt_buf *obj);
```
**Purpose**: Frees packet buffer back to shared memory pool

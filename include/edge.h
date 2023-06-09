#ifndef __EDGE_H__
#define __EDGE_H__

#include "params.h"
#include "buffer.h"
#include "packet.h"
#include "routing_table.h"
// #include "timeseries.h"
// #include "snapshot.h"
// #include "memory.h"

struct edge {
    int16_t edge_index;
    rnode_t routing_table[RTABLE_SIZE];
    //packet buffer data structures
    buffer_t * downstream_pkt_buffer[NODES_PER_RACK];
    buffer_t * upstream_pkt_buffer[NUM_OF_SPINES];
    //track where in upstream_pkt_buffer we have sent snapshots up to
    int16_t snapshot_idx[NUM_OF_SPINES];
    //stats datastructure
    // timeseries_t * downstream_queue_stat[NODES_PER_RACK];
    // timeseries_t * upstream_queue_stat[NUM_OF_SPINES];

};
typedef struct edge* edge_t;

extern edge_t* edges;


edge_t create_edge(int16_t);
// void free_tor(tor_t);
// packet_t send_to_spine_baseline(tor_t, int16_t);
// packet_t send_to_spine(tor_t, int16_t, int64_t *, int64_t *);
// packet_t send_to_spine_dm(tor_t, int16_t, int64_t *, int64_t *);
// packet_t send_to_spine_dram_only(tor_t tor, int16_t spine_id, int64_t * cache_misses);
// packet_t send_to_host_baseline(tor_t, int16_t);
// packet_t send_to_host(tor_t, int16_t, int16_t, int64_t *, int64_t *);
// packet_t send_to_host_dram_only(tor_t tor, int16_t host_within_tor, int64_t * cache_misses);
// snapshot_t * snapshot_to_spine(tor_t, int16_t);
// int64_t tor_up_buffer_bytes(tor_t, int);
// int64_t tor_down_buffer_bytes(tor_t, int);

#endif

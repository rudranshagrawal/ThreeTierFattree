#ifndef __LINKS_H__
#define __LINKS_H__

#include "params.h"
#include "link.h"

struct links {
    link_t host_to_edge_link[NUM_HOSTS][NUM_EDGE_SWITCHES];
    link_t edge_to_agg_link [NUM_EDGE_SWITCHES][NUM_AGGREGATE_SWITCHES];
    link_t agg_to_core_link [NUM_AGGREGATE_SWITCHES][NUM_CORE_SWITCHES];
    link_t core_to_agg_link [NUM_CORE_SWITCHES][NUM_AGGREGATE_SWITCHES];
    link_t agg_to_edge_link [NUM_AGGREGATE_SWITCHES][NUM_EDGE_SWITCHES];
    link_t edge_to_host_link[NUM_EDGE_SWITCHES][NUM_HOSTS];
};

typedef struct links* links_t;

extern links_t links;

links_t create_links();
void free_links(links_t);

#endif

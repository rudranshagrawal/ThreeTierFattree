#include "routing_table.h"

int create_routing_table(rnode_t * routing_table) {
    int i;
    for (i = 0; i < RTABLE_SIZE; i++) {
        routing_table[i].port = -1;
    }
    return i;
}

int hash(rnode_t * routing_table, int key) {
    if (routing_table[key % RTABLE_SIZE].port == -1) {
        int spine = rand() % NUM_OF_SPINES;
        routing_table[key % RTABLE_SIZE].port = spine;
    }
    return routing_table[key % RTABLE_SIZE].port;
}

int hash_agg(rnode_t * routing_table, int key, int pod) {
    int edge_per_pod = NUM_EDGE_SWITCHES / NUM_PODS;
    if (routing_table[key % RTABLE_SIZE].port == -1) {
        int agg = rand() % NUM_PORTS_UP;
        // agg = agg + (edge_per_pod * pod);
        routing_table[key % RTABLE_SIZE].port = agg;
    }
    return routing_table[key % RTABLE_SIZE].port;
}

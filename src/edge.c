#include "edge.h"

edge_t create_edge(int16_t edge_index)
{
    edge_t self = (edge_t) malloc(sizeof(struct edge));
    MALLOC_TEST(self, __LINE__);

    self->edge_index = edge_index;
    int num_port_each_side = NUM_PODS / 2;
    for (int i = 0; i < num_port_each_side; ++i) {
        self->downstream_pkt_buffer[i]
            = create_buffer(DOWNSTREAM_BUFFER_LEN);
        // self->downstream_queue_stat[i] = create_timeseries();
    }

    for (int i = 0; i < num_port_each_side; ++i) {
        self->upstream_pkt_buffer[i]
            = create_buffer(UPSTREAM_BUFFER_LEN);
        // self->upstream_queue_stat[i] = create_timeseries();
        // self->snapshot_idx[i] = 0;
    }
    
    create_routing_table(self->routing_table);
    
    return self;
}

// void free_edge(edge_t self)
// {
//     if (self != NULL) {

//         for (int i = 0; i < NODES_PER_RACK; ++i) {
//             free_buffer(self->downstream_pkt_buffer[i]);
//             free_timeseries(self->downstream_queue_stat[i]);
//         }

//         for (int i = 0; i < NUM_OF_SPINES; ++i) {
//             free_buffer(self->upstream_pkt_buffer[i]);
//             free_timeseries(self->upstream_queue_stat[i]);
//         }

       
//         free(self);
//     }
// }


#include "core.h"

core_t create_core(int16_t core_index)
{
    core_t self = (core_t) malloc(sizeof(struct core));
    MALLOC_TEST(self, __LINE__);

    self->core_index = core_index;

    for (int i = 0; i < NUM_PODS; i++) {
        self->pkt_buffer[i] = create_buffer(CORE_PORT_BUFFER_LEN);
        // self->queue_stat[i] = create_timeseries();
        // self->snapshot_idx[i] = 0;
    }

    return self;
}

void free_core(core_t self)
{
    if (self != NULL) {
        for (int i = 0; i < NUM_PODS; ++i) {
            if (self->pkt_buffer[i] != NULL) {
                free_buffer(self->pkt_buffer[i]);
            }
            // free_timeseries(self->queue_stat[i]);
        }

        free(self);
    }
}
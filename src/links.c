#include "links.h"

links_t create_links()
{
    links_t self = (links_t) malloc(sizeof(struct links));
    MALLOC_TEST(self, __LINE__);

    int k;
    int pod;
    int multiplier = 0;
    int hosts_per_edge = NUM_HOSTS / NUM_EDGE_SWITCHES;
    int edge_per_pod = NUM_EDGE_SWITCHES / NUM_PODS;
    int agg_per_pod = NUM_AGGREGATE_SWITCHES / NUM_PODS;

    // connect hosts to edges upstream
    for (int i = 0; i < NUM_EDGE_SWITCHES; ++i) {
        for (int j = 0; j < hosts_per_edge; ++j) {
            k = j + (hosts_per_edge * i);
            self->host_to_edge_link[k][i] = create_link(k, i, LINK_CAPACITY);
            printf("Link Created -- Host %d -> Edge %d\n", k, i);
        }
    }
        
    // connect edges to hosts downstream
    for (int i = 0; i < NUM_EDGE_SWITCHES; ++i) {
        for (int j = 0; j < hosts_per_edge; ++j) {
            k = j + (hosts_per_edge * i);
            self->edge_to_host_link[i][k] = create_link(i, k, LINK_CAPACITY);
            // printf("Link Created -- Egde %d -> Host %d\n", i, k);
        }
    }

    // connect edges to aggregates upstream
    pod = -1;
    for (int i = 0; i < NUM_AGGREGATE_SWITCHES; ++i) {
        if (i % edge_per_pod == 0) pod++;
        for (int j = 0; j < edge_per_pod; ++j) {
            k = j + (edge_per_pod * pod);
            self->edge_to_agg_link[k][i] = create_link(k, i, LINK_CAPACITY);
            // printf("Link Created -- Egde %d -> Agg %d\n", k, i);
        }
    }

    // connect aggregates to edges downstream
    pod = -1;
    for (int i = 0; i < NUM_AGGREGATE_SWITCHES; ++i) {
        if (i % edge_per_pod == 0) pod++;
        for (int j = 0; j < edge_per_pod; ++j) {
            k = j + (edge_per_pod * pod);
            self->agg_to_edge_link[i][k] = create_link(i, k, LINK_CAPACITY);
            // printf("Link Created -- Agg %d -> Edge %d\n", i, k);
        }
    }

    // connect aggregates to cores upstream
    for (int i = 0; i < NUM_AGGREGATE_SWITCHES; ++i) {
        if (i % agg_per_pod == 0) multiplier = 0;
        for (int j = 0; j < NUM_PORTS_UP; ++j) {
            k = j + (multiplier * NUM_PORTS_UP) ;
            self->agg_to_core_link[i][k] = create_link(i, k, LINK_CAPACITY);
            // printf("Link Created -- Agg %d -> Core %d\n", i, k);
        }
        multiplier++;
    }


    // connect cores to aggregates downstream
    for (int i = 0; i < NUM_AGGREGATE_SWITCHES; ++i) {
        if (i % agg_per_pod == 0) multiplier = 0;
        for (int j = 0; j < NUM_PORTS_UP; ++j) {
            k = j + (multiplier * NUM_PORTS_UP) ;
            self->core_to_agg_link[k][i] = create_link(k, i, LINK_CAPACITY);
            // printf("Link Created -- Core %d -> Agg %d\n", k, i);
        }
        multiplier++;
    }

    return self;
}

void free_links(links_t self)
{
    if (self != NULL) {
    
    int k;
    int pod;
    int multiplier = 0;
    int hosts_per_edge = NUM_HOSTS / NUM_EDGE_SWITCHES;
    int edge_per_pod = NUM_EDGE_SWITCHES / NUM_PODS;
    int agg_per_pod = NUM_AGGREGATE_SWITCHES / NUM_PODS;

    // free hosts to edges upstream
    for (int i = 0; i < NUM_EDGE_SWITCHES; ++i) {
        for (int j = 0; j < hosts_per_edge; ++j) {
            k = j + (hosts_per_edge * i);
            free_link(self->host_to_edge_link[k][i]);
            // printf("Link Freed -- Host %d -> Edge %d\n", k, i);
        }
    }
        
    // free edges to hosts downstream
    for (int i = 0; i < NUM_EDGE_SWITCHES; ++i) {
        for (int j = 0; j < hosts_per_edge; ++j) {
            k = j + (hosts_per_edge * i);
            free_link(self->edge_to_host_link[i][k]);
            // printf("Link Freed -- Egde %d -> Host %d\n", i, k);
        }
    }

    // free edges to aggregates upstream
    pod = -1;
    for (int i = 0; i < NUM_AGGREGATE_SWITCHES; ++i) {
        if (i % edge_per_pod == 0) pod++;
        for (int j = 0; j < edge_per_pod; ++j) {
            k = j + (edge_per_pod * pod);
            free_link(self->edge_to_agg_link[k][i]);
            // printf("Link Created -- Egde %d -> Agg %d\n", k, i);
        }
    }

    // free aggregates to edges downstream
    pod = -1;
    for (int i = 0; i < NUM_AGGREGATE_SWITCHES; ++i) {
        if (i % edge_per_pod == 0) pod++;
        for (int j = 0; j < edge_per_pod; ++j) {
            k = j + (edge_per_pod * pod);
            free_link(self->agg_to_edge_link[i][k]);
            // printf("Link Created -- Agg %d -> Edge %d\n", i, k);
        }
    }

    // free aggregates to cores upstream
    for (int i = 0; i < NUM_AGGREGATE_SWITCHES; ++i) {
        if (i % agg_per_pod == 0) multiplier = 0;
        for (int j = 0; j < NUM_PORTS_UP; ++j) {
            k = j + (multiplier * NUM_PORTS_UP) ;
            free_link(self->agg_to_core_link[i][k]);
            // printf("Link Created -- Agg %d -> Core %d\n", i, k);
        }
        multiplier++;
    }


    // free cores to aggregates downstream
    for (int i = 0; i < NUM_AGGREGATE_SWITCHES; ++i) {
        if (i % agg_per_pod == 0) multiplier = 0;
        for (int j = 0; j < NUM_PORTS_UP; ++j) {
            k = j + (multiplier * NUM_PORTS_UP) ;
            free_link(self->core_to_agg_link[k][i]);
            // printf("Link Created -- Core %d -> Agg %d\n", k, i);
        }
        multiplier++;
    }

        free(self);
    }
}

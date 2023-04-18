#include "driver.h"

// Default values for simulation
static int pkt_size = MTU; //in bytes
static float link_bandwidth = 100; //in Gbps
static float timeslot_len = 120; //in ns
static int bytes_per_timeslot = 1500;
static int snapshot_epoch = 1; //timeslots between snapshots

int16_t header_overhead = 64;
float per_hop_propagation_delay_in_ns = 100;
int per_hop_propagation_delay_in_timeslots;
int dram_access_time = DRAM_DELAY;
int timeslots_per_dram_access = 1;
int accesses_per_timeslot = 1;
volatile int64_t curr_timeslot = 0; //extern var
int packet_counter = 0;
int num_datapoints = 100000;

int enable_sram = 1; // Value of 1 = Enable SRAM usage
int init_sram = 0; // Value of 1 = initialize SRAMs
int program_tors = 1; // Value of 1 = ToRs are programmable
int fully_associative = 1; // Value of 1 = use fully-associative SRAM
int64_t sram_size = (int64_t) SRAM_SIZE;
int burst_size = 3; // Number of packets to send in a burst
int packet_mode = 0; // 0: Full network bandwidth mode, 1: incast mode, 2: burst mode
int incast_active = 0; // Number of flows actively sending incast packets. Remaining nodes are reserved for switching
int incast_switch = 1; // Number of flows that switch flows for each incast period
int incast_size = 1; // Number of timeslots to send incast packets
int incast_pause = 1; // Number of timeslots to wait for buffers to drain
int incast_period; // Period of an incast burst
double load = 1.0; // Network load 
int64_t cache_misses = 0;
int64_t cache_hits = 0;
int64_t spine_cache_misses = 0;
int64_t spine_cache_hits = 0;
int64_t total_bytes_rcvd = 0;
int64_t total_pkts_rcvd = 0;
float avg_delay_at_spine = 0;
float avg_max_delay_at_spine = 0;
int max_delay_in_timeperiod = 0;
int64_t pkt_delays_recorded = 0;

static volatile int8_t terminate0 = 0;
static volatile int8_t terminate1 = 0;
static volatile int8_t terminate2 = 0;
static volatile int8_t terminate3 = 0;
static volatile int8_t terminate4 = 0;

volatile int64_t num_of_flows_finished = 0; //extern var
int64_t num_of_flows_to_finish = 500000; //stop after these many flows finish

volatile int64_t total_flows_started = 0; //extern var
int64_t num_of_flows_to_start = 500000; //stop after these many flows start

volatile int64_t max_timeslots = 30000; // extern var

int edge_per_pod = NUM_EDGE_SWITCHES / NUM_PODS;

// Output files
FILE * out_fp = NULL;
FILE * timeseries_fp = NULL;
FILE * spine_outfiles[NUM_OF_SPINES];
FILE * tor_outfiles[NUM_OF_RACKS];

// Network
node_t * nodes;
edge_t * edges;
aggregate_t * aggregates;
core_t* cores;
links_t links;
flowlist_t * flowlist;

void work_per_timeslot()
{
    int flow_idx = 0;

    while(1)
    {
        /*---------------------------------------------------------------------------*/
                                // ACTIVATE INACTIVE FLOWS
        /*---------------------------------------------------------------------------*/
        for (; flow_idx < flowlist->num_flows; flow_idx++) {
            flow_t * flow = flowlist->flows[flow_idx];
            if (flow == NULL || flow->timeslot > curr_timeslot) {
                break;
            }
            else if (flow->timeslot == curr_timeslot) {
            #ifdef DEBUG_DRIVER
                printf("%d: flow %d has been enabled\n", (int) curr_timeslot, (int) flow->flow_id);
            #endif
                int src = flow->src;
                buffer_put(nodes[src]->active_flows, flow);
            }
        }

        /*---------------------------------------------------------------------------*/
                                        // HOST SEND
        /*---------------------------------------------------------------------------*/
        for (int i = 0; i < NUM_HOSTS; ++i) {

                node_t node = nodes[i];
                int16_t node_index = node->node_index;
                // Check if host has any active flows at current moment
                // printf("Active Flows for Node %d are %d\n", node_index, (node->active_flows)->num_elements);
                if ((node->active_flows)->num_elements > 0 || node->current_flow != NULL) {
                    flow_t * flow = NULL;
                    // Time to select a new burst flow
                    if (curr_timeslot % burst_size == 0 || node->current_flow == NULL) {
                        // printf("curr timeslot %d \n", curr_timeslot);
                        // Return the current flow back to the active flows list
                        if (node->current_flow != NULL) {
                            buffer_put(node->active_flows, node->current_flow);
                            node->current_flow = NULL;
                        }
                        // Randomly select a new active flow to start
                        int32_t num_af = node->active_flows->num_elements;
                        // printf("Number of Active Flows %d\n", num_af);
                        int32_t selected = (int32_t) rand() % num_af;
                        flow = buffer_remove(node->active_flows, selected);
                        // If flow is not active yet, return it to flow list
                        if (flow == NULL || (flow->active == 0 && flow->timeslot > curr_timeslot)) {
                            buffer_put(node->active_flows, flow);
                            flow = NULL;
                        }
                        else {
                            node->current_flow = flow;
                        }
                    }
                    // Burst size has not been reached; keep sending from current flow
                    else {
                        flow = node->current_flow;
                    }
                    
                

                // Send packet if random generates number below load ratio
                if ((double) rand() / (double) RAND_MAX < load) {        
                    if (flow != NULL && (flow->active == 1 || flow->timeslot <= curr_timeslot)) {
                        int16_t src_node = flow->src;
                        int16_t dst_node = flow->dst;
                        int64_t flow_id = flow->flow_id;

                        // Send packets from this flow until cwnd is reached or the flow runs out of bytes to send
                        int64_t flow_bytes_remaining = flow->flow_size_bytes - flow->bytes_sent;
                        int64_t cwnd_bytes_remaining = node->cwnd[dst_node] * MTU - (node->seq_num[flow_id] - node->last_acked[flow_id]);
                        if (flow_bytes_remaining > 0 && cwnd_bytes_remaining > 0) {
                            // Determine how large the packet will be
                            int64_t size = MTU;
                            if (cwnd_bytes_remaining < MTU) {
                                size = cwnd_bytes_remaining;
                            }
                            if (flow_bytes_remaining < size) {
                                size = flow_bytes_remaining;
                            }
                            assert(size > 0);

                            // Create packet
                            packet_t pkt = create_packet(src_node, dst_node, flow_id, size, node->seq_num[flow_id], packet_counter);
                            packet_counter++;
                            node->seq_num[dst_node] += size;

                            // Send packet
                            pkt->time_when_transmitted_from_src = curr_timeslot;
                            int total_ports = NUM_PORTS_DOWN;
                            int16_t dst_tor = (node_index) / total_ports;
                            // printf("DST TOR %d\n",dst_tor);
                            pkt->time_to_dequeue_from_link = curr_timeslot +
                                per_hop_propagation_delay_in_timeslots;
                            link_enqueue(links->host_to_edge_link[node_index][dst_tor], pkt);
                            // print_packet(pkt);
    #ifdef DEBUG_DRIVER
                            printf("%d: host %d created and sent pkt to ToR %d\n", (int) curr_timeslot, i, (int) dst_tor);
                            
    #endif

                            // Update flow state
                            if (flow->active == 0) {
                                flowlist->active_flows++;
                                flow->start_timeslot = curr_timeslot;
                                total_flows_started++;
    #ifdef DEBUG_DRIVER 
                                printf("%d: flow %d started\n", (int) curr_timeslot, (int) flow_id);
    #endif
                            }

                            flow->active = 1;
                            flow->pkts_sent++;
                            flow->bytes_sent += size;
                            flow->timeslots_active++;
                            // Determine if last packet of flow has been sent
                            if (flow->pkts_sent >= flow->flow_size) {
                                flow->active = 0;
                                flowlist->active_flows--;
    #ifdef DEBUG_DRIVER
                                printf("%d: flow %d sending final packet\n", (int) curr_timeslot, (int) flow_id);
    #endif
                            }
                        }

                        // Set current flow back to null if there are no more bytes left to send from this flow
                        if (flow_bytes_remaining < 1) {
                            node->current_flow = NULL;
                        }
                        
                        // Return flow back to active flows if it is not complete
                        //if (flow_bytes_remaining > 0) {
                        //    buffer_put(node->active_flows, flow);
                        //    node->current_flow = NULL;
                        //}
                    }
                }
            }
        }

        printf("HOST SEND FININSHED\n");

        /*---------------------------------------------------------------------------*/
                                        // HOST RECV
        /*---------------------------------------------------------------------------*/
        for (int i = 0; i < NUM_HOSTS; ++i) {
            node_t node = nodes[i];
            int16_t node_index = node->node_index;
            int total_ports = NUM_PORTS_DOWN;
            //deq packet
            int16_t src_edge = node_index / total_ports;
            int bytes_rcvd = 0;
            packet_t peek_pkt = (packet_t) link_peek(links->edge_to_host_link[src_edge][node_index]);
            while (peek_pkt != NULL && peek_pkt->time_to_dequeue_from_link == curr_timeslot && bytes_rcvd + peek_pkt->size <= bytes_per_timeslot) {
                assert(peek_pkt->dst_node == node_index);
                packet_t pkt = (packet_t) link_dequeue(links->edge_to_host_link[src_edge][node_index]);
                // Data Packet
                if (pkt->control_flag == 0) {
#ifdef DEBUG_DRIVER
                    printf("%d: host %d received data packet from ToR %d\n", (int) curr_timeslot, node_index, src_tor);
#endif
                    // Create ACK packet
                    node->ack_num[pkt->flow_id] = pkt->seq_num + pkt->size;
                    packet_t ack = ack_packet(pkt, node->ack_num[pkt->flow_id]);
                    // Respond with ACK packet
                    ack->time_when_transmitted_from_src = curr_timeslot;
                    
                    int16_t dst_tor = (node_index) / total_ports;
                    ack->time_to_dequeue_from_link = curr_timeslot +
                        per_hop_propagation_delay_in_timeslots;
                    link_enqueue(links->host_to_edge_link[node_index][dst_tor], ack);
#ifdef DEBUG_DCTCP
                    printf("%d: host %d created and sent ACK %d to dst %d\n", (int) curr_timeslot, i, (int) ack->ack_num, (int) pkt->src_node);
#endif

                    // Update flow
                    flow_t * flow = flowlist->flows[pkt->flow_id];
                    assert(flow != NULL);
                    flow->pkts_received++;
                    flow->bytes_received += pkt->size;
                    total_bytes_rcvd += pkt->size;
                    total_pkts_rcvd++;

                    if (flow->pkts_received == flow->flow_size) {
                        assert(flow->bytes_sent == flow->bytes_received);
                        flow->active = 0;
                        flow->finished = 1;
                        flow->finish_timeslot = curr_timeslot;
                        num_of_flows_finished++;
                        // write_to_outfile(out_fp, flow, timeslot_len, link_bandwidth);
#ifdef DEBUG_DRIVER
                        printf("%d: Flow %d finished\n", (int) curr_timeslot, (int) flow->flow_id);
#endif
                    }
                }
                // Control Packet
                else {
#ifdef DEBUG_DCTCP
                    printf("%d: host %d received control packet from dst %d\n", (int) curr_timeslot, node_index, pkt->src_node);
#endif
                    // Check ECN flag
                    track_ecn(node, pkt->flow_id, pkt->ecn_flag);

                    // Check ACK value
                    if (pkt->ack_num > node->last_acked[pkt->flow_id]){
                        node->last_acked[pkt->flow_id] = pkt->ack_num;
                    }
                }
                bytes_rcvd += pkt->size;
                peek_pkt = (packet_t) link_peek(links->edge_to_host_link[src_edge][node_index]);
                free_packet(pkt);
            }
        }


        /*---------------------------------------------------------------------------*/
                                  //EDGE -- RECV FROM HOST AND AGGREGATE 
        /*---------------------------------------------------------------------------*/
        
        int pod = -1;
        for (int i = 0; i <  NUM_EDGE_SWITCHES; ++i) {
            edge_t edge = edges[i];
            int16_t edge_index = edge->edge_index;
            if (i % edge_per_pod == 0) pod++;

            //Recv packet from host
            for (int edge_port = 0; edge_port < NUM_PORTS_DOWN; ++edge_port) {
                //deq packet from the link
                int16_t src_host = (edge_index * NUM_PORTS_DOWN) + edge_port;
                int bytes_rcvd = 0;
                packet_t peek_pkt = (packet_t) link_peek(links->host_to_edge_link[src_host][edge_index]);
                while (peek_pkt != NULL && peek_pkt->time_to_dequeue_from_link == curr_timeslot && bytes_rcvd + peek_pkt->size <= bytes_per_timeslot) {
                    int agg_id = hash_agg(edge->routing_table, peek_pkt->flow_id, pod);
                    printf("agg id %d\n", agg_id + (edge_per_pod * pod));
                    packet_t pkt = (packet_t)
                        link_dequeue(links->host_to_edge_link[src_host][edge_index]);
                        print_packet(pkt);
                    if (pkt != NULL) {
                        buffer_put(edge->upstream_pkt_buffer[agg_id], pkt);
                        // DCTCP: Mark packet when adding packet exceeds ECN cutoff
                        if (edge->upstream_pkt_buffer[agg_id]->num_elements > ECN_CUTOFF_TOR_UP) {
                            pkt->ecn_flag = 1;
                        }
                        bytes_rcvd += pkt->size;
                    }
                    peek_pkt = (packet_t) link_peek(links->host_to_edge_link[src_host][edge_index]);
                }
            }

//             //Recv packet from aggregate
            for (int edge_port = 0; edge_port < NUM_PORTS_UP; ++edge_port) {
                //deq packet from the link
                int16_t src_agg = edge_port + (edge_per_pod * pod);
                int bytes_rcvd = 0;
                packet_t peek_pkt = (packet_t) link_peek(links->agg_to_edge_link[src_agg][edge_index]);
                while (peek_pkt != NULL && peek_pkt->time_to_dequeue_from_link == curr_timeslot && bytes_rcvd + peek_pkt->size <= bytes_per_timeslot) {
                    packet_t pkt = (packet_t)
                        link_dequeue(links->agg_to_edge_link[src_agg][edge_index]);
                    if (pkt != NULL) {
                        int node_index = pkt->dst_node % NUM_PORTS_DOWN;
                        buffer_put(edge->downstream_pkt_buffer[node_index], pkt);
                        // DCTCP: Mark packet when adding packet exceeds ECN cutoff
                        if (edge->downstream_pkt_buffer[node_index]->num_elements > ECN_CUTOFF_TOR_DOWN) {
                            pkt->ecn_flag = 1;
#ifdef DEBUG_DCTCP
                            printf("%d: ToR %d marked downstream pkt with ECN\n", (int) curr_timeslot, i);
#endif
                        }
#ifdef DEBUG_DRIVER
                        printf("%d: Tor %d recv pkt from spine %d\n", (int) curr_timeslot, i, tor_port);
#endif
#ifdef RECORD_PACKETS
                        fprintf(tor_outfiles[i], "%d, %d, %d, %d, %d, down\n", (int) pkt->flow_id, (int) pkt->src_node, (int) pkt->dst_node, (int) tor_port, (int) (curr_timeslot * timeslot_len));
#endif
                        bytes_rcvd += pkt->size;
                    }
                    peek_pkt = (packet_t) link_peek(links->agg_to_edge_link[src_agg][edge_index]);
                }
                // Receive snapshot
//                 snapshot_t * snapshot = (snapshot_t * ) ipg_peek(links->spine_to_agg_link[src_spine][edge_index]);
//                 if (snapshot != NULL && snapshot->time_to_dequeue_from_link == curr_timeslot) {
//                     snapshot = ipg_recv(links->spine_to_agg_link[src_spine][tor_index]);
//                     free(snapshot);
// #ifdef DEBUG_SNAPSHOTS
//                     printf("%d: ToR %d recv snapshot from spine %d\n", (int) curr_timeslot, i, tor_port);
// #endif
//                 }
            }
        }





        if (curr_timeslot > 10) {
            double curr_time = curr_timeslot * timeslot_len / 1e9;
            printf("Finished in %d timeslots\n", (int) curr_timeslot);
            printf("Finished in %f seconds\n", curr_time);
            break;
        }

         if (curr_timeslot % 100 == 0) {
            printf(".");
        }
        if (curr_timeslot % 10000 == 9999) {
            printf("\n");
        }

        curr_timeslot++;
        
    }
}

void read_tracefile(char * filename) {
    FILE * fp;
    flowlist = create_flowlist();
    if (strcmp(filename, "")) {
        printf("Opening tracefile %s\n", filename);
        fp = fopen(filename, "r");
        if (fp == NULL) {
            perror("open");
            exit(1);
        }
        int flow_id = -1;
        int src = -1;
        int dst = -1;
        int flow_size_bytes = -1;
        int flow_size_pkts = -1;
        int timeslot = -1;
        
        while (fscanf(fp, "%d,%d,%d,%d,%d,%d", &flow_id, &src, &dst, &flow_size_bytes, &flow_size_pkts, &timeslot) == 6 && flow_id < MAX_FLOW_ID) {
            flow_size_pkts = flow_size_bytes / MTU;
            initialize_flow(flow_id, src, dst, flow_size_pkts, flow_size_bytes, timeslot);
            // Final timeslot is start time of last flow
            max_timeslots = timeslot;
        }
        
        printf("Flows initialized\n");
        fclose(fp);
    }
    return;
}

void initialize_flow(int flow_id, int src, int dst, int flow_size_pkts, int flow_size_bytes, int timeslot) {
    if (flow_id < MAX_FLOW_ID) {
        flow_t * new_flow = create_flow(flow_id, flow_size_pkts, flow_size_bytes, src, dst, timeslot);
        add_flow(flowlist, new_flow);
#ifdef DEBUG_DRIVER
        printf("initialized flow %d src %d dst %d flow_size %d bytes %d ts %d\n", flow_id, src, dst, flow_size_pkts, flow_size_bytes, timeslot);
#endif
    }
}

void initialize_network() {
   
   //create hosts
    nodes = (node_t*) malloc(NUM_HOSTS * sizeof(node_t));
    MALLOC_TEST(nodes, __LINE__);
    for (int i = 0; i < NUM_HOSTS; ++i) {
        nodes[i] = create_node(i);
    }
    printf("Nodes initialized\n");

    //create edge switches
    edges = (edge_t*) malloc(NUM_EDGE_SWITCHES * sizeof(edge_t));
    MALLOC_TEST(edges, __LINE__);
    for (int i = 0; i < NUM_EDGE_SWITCHES; ++i) {
        edges[i] = create_edge(i);
    }
    printf("Edges initialized\n");

    //create aggregate switches
    aggregates = (aggregate_t*) malloc(NUM_AGGREGATE_SWITCHES * sizeof(aggregate_t));
    MALLOC_TEST(aggregates, __LINE__);
    for (int i = 0; i < NUM_AGGREGATE_SWITCHES; ++i) {
        aggregates[i] = create_aggregate(i);
    }
    printf("Aggregates initialized\n");

    //create core switches
    cores = (core_t*) malloc(NUM_CORE_SWITCHES * sizeof(core_t));
    MALLOC_TEST(cores, __LINE__);
    for (int i = 0; i < NUM_CORE_SWITCHES; ++i) {
        cores[i] = create_core(i);
    }
    printf("Cores initialized\n");
    

    //create links
    links = create_links();
    printf("Links initialized\n");


}




int main(int argc, char** argv) {
    srand((unsigned int)time(NULL));

    initialize_network();
    // process_args(argc, argv);
    
    read_tracefile("trace00.csv.processed");
    printf("Tracefile processed\n");
    
    work_per_timeslot();
    // print_system_stats(spines, tors, total_bytes_rcvd, total_pkts_rcvd, (int64_t) (curr_timeslot * timeslot_len), cache_misses, cache_hits);
    // free_flows();
    // free_network();
    // close_outfiles();
    free_links(links);
    

    printf("Finished execution\n");

    return 0;
}

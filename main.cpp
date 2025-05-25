#include <getopt.h>
#include <iostream>
#include <numa.h>
#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "MdReceiver.h"
#include "config.h"

// заколотить в NUMA-узел 0 (память + CPU)
void pin_to_numa0()
{
    if (numa_available() < 0) {
        fprintf(stderr, "NUMA is not available\n");
        return;
    }

    // Привязка выделения памяти к NUMA-узлу 0
    numa_run_on_node(0);
    numa_set_preferred(0);

    // Привязка текущего треда к первому CPU NUMA-узла 0
    struct bitmask *cpus = numa_allocate_cpumask();
    if (numa_node_to_cpus(0, cpus) < 0) {
        perror("numa_node_to_cpus");
        return;
    }

    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    for (size_t i = 0; i < cpus->size; ++i) {
        if (numa_bitmask_isbitset(cpus, i)) {
            CPU_SET(i, &cpu_set);
        }
    }
    numa_free_cpumask(cpus);

    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set) < 0) {
        perror("sched_setaffinity");
    }
}

int main(int argc, char **argv)
{
    if (argc != 7) {
        std::cerr << "Usage: solution <header_path_in> <buffer_path_in> <header_path_out> <buffer_path_out> "
                     "<buffer_size> <meta_path>"
                  << std::endl;
        return 1;
    }

    pin_to_numa0();

    std::string header_path_in = argv[1];
    std::string buffer_path_in = argv[2];
    std::string header_path_out = argv[3];
    std::string buffer_path_out = argv[4];
    size_t buffer_size = std::stoul(argv[5]);
    std::string meta_path = argv[6];

    MdConfig md_config(meta_path);
    SPSCQueue in_queue(header_path_in, buffer_path_in, buffer_size, true);
    SPSCQueue out_queue(header_path_out, buffer_path_out, buffer_size, false);
    MdReceiver rcvrManager(md_config, in_queue, out_queue);

    rcvrManager.start();

    return 0;
}
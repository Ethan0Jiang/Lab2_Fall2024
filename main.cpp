// main.cpp
//   by Derek Chiou
//      March 4, 2007
// 
//   modified October 22, 2009

// for 382N-10

// high-level structure, initialization, and argument processing

// STUDENTS: YOU ARE NOT ALLOWED TO MODIFY THIS FILE.  

#include<stdio.h>
#include<stdlib.h>

#include "helpers.h"
#include "cache.h"
#include "iu.h"
#include "network.h"
#include "test.h"

// pointers to all components of the simulator
proc_t    **procs;  // array of pointers to processors
cache_t   **caches; // array of pointers to caches
iu_t      **ius; // array of pointers to IUs
network_t *network; // pointer to network, only one network

int cur_cycle = 0; // current cycle

uint gen_node_mask = 0x0; // to be defined in parse_args
uint gen_local_addr_shift;

args_t args;

void advance_time() {
  static int p = 0;

  for (int i = 0; i < args.num_procs; ++i) { // advance all processors by one cycle
    p = (p + 1) % args.num_procs;

    ius[p]->advance_one_cycle();
    procs[p]->advance_one_cycle();

  }
  ++cur_cycle;
}


void parse_args(int argc, char *argv[]) {
  if (argc != 5) {
    ERROR("usage: <number of processors> <num cycles> <test> <verbose>");
  }
  
  args.num_procs = atoi(argv[1]);
  args.num_cycles = atoi(argv[2]);
  args.test = atoi(argv[3]);
  args.verbose = atoi(argv[4]);

  gen_node_mask = (1 << (lg(args.num_procs))) - 1;
  gen_local_addr_shift = lg(args.num_procs) + LG_INTERLEAVE_SIZE;
}

void init_system() {
  network = new network_t(args.num_procs); // only one network, build with number of processors
  procs = new proc_t *[args.num_procs]; // * means array of pointers, each pointer points to a proc_t which is a processor
  caches = new cache_t *[args.num_procs]; // array of pointers to caches
  ius = new iu_t*[args.num_procs]; // array of pointers to IUs

  for (int p = 0; p < args.num_procs; ++p) {
    procs[p] = new proc_t(p);  // create a processor with id p
    caches[p] = new cache_t(p, 2, 3, LG_CACHE_LINE_SIZE);  // create a cache with id p, 2 means 4 ways, 3 means 8 sets, LG_CACHE_LINE_SIZE is 3 means 8 words per cache line
    ius[p] = new iu_t(p);;
    
    procs[p]->bind(caches[p]);  // bind processor to cache, bind means connect
    caches[p]->bind(ius[p]);  // bind cache to IU
    ius[p]->bind(caches[p], network); // bind IU to cache and network
  }

  init_test();
}

main(int argc, char *argv[]) {
  int cycle;

  parse_args(argc, argv);
  init_system(); // initialize the system, also initialize the test
  
  for (cycle = 0; cycle < args.num_cycles; ++cycle) { // run for num_cycles
    advance_time();  // advance time by one cycle
  }

  for (int i = 0; i < args.num_procs; ++i) {  // print stats for each processor at the end
    caches[i]->print_stats();
    ius[i]->print_stats();
  }

  finish_test(); 
  
}

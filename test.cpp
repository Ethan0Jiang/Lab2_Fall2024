// test.cpp
//   Derek Chiou
//     Oct. 8, 2023


#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "generic_error.h"
#include "helpers.h"
#include "cache.h"
#include "test.h"

proc_t::proc_t(int __p) {
  proc = __p;
  init();
}

void proc_t::init() {
  response.retry_p = false;
  ld_p = false;
}

void proc_t::bind(cache_t *c) {
  cache = c;
}


// ***** FYTD ***** 

// this is just a simple random test.  I'm not giving
// you any more test cases than this.  You will be tested on the
// correctness and performance of your solution.

extern args_t args;
extern int addr_range;
extern cache_t **caches;

test_args_t test_args;

void init_test() {
  switch(args.test) {
  case 0:
    test_args.addr_range = 8192;  // addr range is # cache lines * words per cache line * number of processors = 1024* 8 * 1  = 8192 in case 0
    // test_args.addr_range = 512；
    break;

  case 1: // 4 cores test
  test_args.addr_range = 8192 * 4;

  case 2: // 4 cores test
  test_args.addr_range = 8192 * 4;

  default:
    ERROR("don't recognize this test");
  }
}

void finish_test() {
  double hr;

  for (int i = 0; i < args.num_procs; ++i) {
    switch(args.test) {
    case 0:
      hr = caches[i]->hit_rate();
      if (!within_tolerance(hr, 0.5, 0.01)) { 
	      ERROR("out of tolerance");
      }
      break;


    case 1:
      hr = caches[i]->hit_rate();
      printf("Processor %d hit rate: %.2f\n", i, hr);
      break;

    case 2:
      hr = caches[i]->hit_rate();
      printf("Processor %d hit rate: %.2f\n", i, hr);
      break;
      
    default: 
      ERROR("don't recognize this test");
    }
  }
  printf("passed\n");
}

void proc_t::advance_one_cycle() {
  int data;
  // a atatic operation counter
  static int op_count = 0;

  switch (args.test) {
  case 0:
    if (!response.retry_p) {
      addr = random() % test_args.addr_range;
      ld_p = ((random() % 2) == 0);  // 50% chance of load
    }
    if (ld_p) response = cache->load(addr, 0, &data, response.retry_p);
    else      response = cache->store(addr, 0, cur_cycle, response.retry_p);
    break;


  // A random one but with some print statements (similar to case 0, but with a larger address space)
  case 1:
    if (!response.retry_p) {
      addr = random() % test_args.addr_range;
      ld_p = ((random() % 2) == 0); 
    }
    if (ld_p) {
      // Perform a load operation
      response = cache->load(addr, 0, &data, response.retry_p);
      if (args.verbose) {
        printf("Processor %d: Loading from address %d, data: %d\n", proc, addr, data);
      }
    } else {
      // Perform a store operation with current cycle as data
      response = cache->store(addr, 0, cur_cycle, response.retry_p);
      if (args.verbose) {
        printf("Processor %d: Storing to address %d, data: %d\n", proc, addr, cur_cycle);
      }
    }
    break;


    case 2:
      if (proc < 3) {  // 3 processors share the same address
        if (!response.retry_p) {
          addr = 324; // Fixed address
          response = cache->load(addr, 0, &data, response.retry_p);
        }
        if (args.verbose) {
          printf("Processor %d: Loading from address %d, data: %d\n", proc, addr, data);
        }
      } else if (proc == 3) { // Processor 3 stores to different addresses with same set, need eviction, also test snooping and writeback
        int set_shift = LG_CACHE_LINE_SIZE;
        int lg_num_sets = 3;
        int set_mask = (1 << lg_num_sets) - 1;
        int set = (addr >> set_shift) & set_mask;
        // Different addresses with same set, random address but with set bits replace back
        if (!response.retry_p || op_count < 4) {
          addr = (random() % 8192) & ~(set_mask << set_shift); // Clear set bits
          addr |= (set << set_shift); // Apply the calculated set bits
          op_count++;
          response = cache->store(addr, 0, cur_cycle, response.retry_p);
        } else if (!response.retry_p || op_count == 4) {
          addr = 324; // Fixed address
          op_count = 0;
          response = cache->store(addr, 0, cur_cycle, response.retry_p);
        } 
        if (args.verbose) {
          printf("Processor %d: Storing to address %d, data: %d\n", proc, addr, cur_cycle);
        }
      }
      break;
        

  default:
    ERROR("don't know this test case");
  }
}

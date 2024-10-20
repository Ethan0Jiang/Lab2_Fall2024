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
    test_args.addr_range = 512;
    break;

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
      
    default: 
      ERROR("don't recognize this test");
    }
  }
  printf("passed\n");
}

void proc_t::advance_one_cycle() {
  int data;

  switch (args.test) {
  case 0:
    if (!response.retry_p) {
      addr = random() % test_args.addr_range;
      ld_p = ((random() % 2) == 0);
    }
    if (ld_p) response = cache->load(addr, 0, &data, response.retry_p);
    else      response = cache->store(addr, 0, cur_cycle, response.retry_p);
    break;

  default:
    ERROR("don't know this test case");
  }
}

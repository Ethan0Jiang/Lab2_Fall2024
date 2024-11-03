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
  store_done = false;
  load_done = false;
  final_load_done = false;
  initial_load_done = false;
  retry_count = 0;
  max_retry_count = 0;

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

typedef struct{
        int op; //0 is store, 1 is load
        address_t addr;
        int data;
      } instruction_t;

instruction_t program[32][50];
int pc[32];

void init_test() {
  switch(args.test) {
  case 0:
    // test_args.addr_range = 8192;  // addr range is # cache lines * words per cache line * number of processors = 1024* 8 * 1  = 8192 in case 0
    // test_args.addr_range = 512;  // if set at 512 with 4 cores, only core 0 and 1 will hit local
    test_args.addr_range = 1024 * 32; 
    break;

  case 1: // 4 cores test
    // test_args.addr_range = 8192 * 4;
    test_args.addr_range = 256 * 32;
    break;

  case 2: // 4 cores test
    // test_args.addr_range = 8192 * 4;
    test_args.addr_range = 256 * 4;
    break;

  case 3: // multiple cores test
    // test_args.addr_range = 8192 * 4;
    test_args.addr_range = 256 * 4;
    // for(int i = 0; i<2; i++){
    //   for(int j = 0; j<args.num_procs; j++){
    //     if(i == 0){
    //       if(j==0){
    //         program[j][i] = (instruction_t){0, (args.num_procs*256) - 1, j};
    //       }
    //       else{
    //         program[j][i] = (instruction_t){0, (j*256)-1, j};
    //       }
    //     }
    //     if(i==1)
    //       program[j][i] = (instruction_t){1, ((j+1)*256) -1};
    //   }
    // }
    break;

  case 4: // PRI3 race condition test
    for(int i = 0; i<args.num_procs; i++){
      pc[i] = 0;
    }
    test_args.addr_range = 512*4;
    for(int i = 0; i<50; i++){
      for(int j = 0; j<args.num_procs; j++){
        if(i==0){
          if(j==0){
            program[j][i] = (instruction_t){0, (args.num_procs*256) - 1, j};
          }
          else{
            program[j][i] = (instruction_t){0, (j*256)-1, j};
          }
        }
        if(i==1)
          program[j][i] = (instruction_t){1, ((j+1)*256)-1, 0};
        if(i==2)
          program[j][i] = (instruction_t){0, j, 69420+j}; //store 69420 to address j, node 0 is homesite.
        if(i==3)
          program[j][i] = (instruction_t){1, j, 0};
      }
    }
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

    case 1:
      hr = caches[i]->hit_rate();
      printf("Processor %d hit rate: %.2f\n", i, hr);
      break;

    case 2:
      hr = caches[i]->hit_rate();
      printf("Processor %d hit rate: %.2f\n", i, hr);
      break;

    case 3:
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
  // a static operation counter
  static int op_count = 0;

  switch (args.test) {
  case 0:
    if (!response.retry_p) {
      addr = random() % test_args.addr_range;
      ld_p = ((random() % 2) == 0);  // 50% chance of load
      retry_count = 0;
    } else {
      retry_count++;
      if (retry_count > max_retry_count) {
        max_retry_count = retry_count;
      }
    }
    
    if (ld_p) response = cache->load(addr, 0, &data, response.retry_p);
    else      response = cache->store(addr, 0, cur_cycle, response.retry_p);

    if(cur_cycle == 9999){
      printf("Processor %d: max retry count %d\n", proc, max_retry_count);
      printf("Processor %d: retry count %d\n", proc, retry_count);
    }

    if (retry_count > 500) {  // our's only goes to ~200
      printf("Processor %d: too many retries %d times, maybe deadlock\n", proc, retry_count);
      printf(" the command is: ld_p ==%d, addr==%d, %d\n", ld_p, addr, response.retry_p);
      ERROR("too many retries, maybe deadlock");
    }
  break;


  // A random one but with some print statements (similar to case 0, but with a larger address space)
  case 1:
    // At the beginning, every node except node 0 performs a load to addr 324.
    // They keep attempting until they succeed. Node 0 does nothing initially.
    if (proc > 0) {
      if (!initial_load_done) {
        addr = 324;
        response = cache->load(addr, 0, &data, response.retry_p);
        if (!response.retry_p) {
          initial_load_done = true;
          if (args.verbose) {
            printf("Processor %d: Initial load from address %d, data: %d\n", proc, addr, data);
          }
        }
      }
    }

    // At cycle 50, the node 0 performs a RWITM (store) to addr 324 with data 100.
    if (proc == 0 && cur_cycle >= 50) {
      if (!store_done) {
        addr = 324;
        response = cache->store(addr, 0, 100, response.retry_p);
        if (!response.retry_p) {
          store_done = true;
          if (args.verbose) {
            printf("Processor %d: Wrote data %d to address %d\n", proc, 100, addr);
          }
        }
      }
    }

    // At cycle 100 and beyond, all nodes except node 0 perform a load to addr 324.
    if (proc > 0 && cur_cycle >= 100) {
      if (!final_load_done) {
        addr = 324;
        response = cache->load(addr, 0, &data, response.retry_p);
        if (!response.retry_p) {
          final_load_done = true;
          if (args.verbose) {
            printf("Processor %d: Final load from address %d, data: %d\n", proc, addr, data);
            // the data must be 100
            if (data != 100) {
              ERROR("data is not 100");
            }
          }
        }
      }
    }

  break;


  case 2:
    if (proc < args.num_procs-1) { 
      if (!response.retry_p) {
        addr = 324; // Fixed address
      }        
      response = cache->load(addr, 0, &data, response.retry_p);
      if (args.verbose) {
        printf("Processor %d: Loading from address %d, data: %d\n", proc, addr, data);
      }
    } else if (proc == args.num_procs-1) { // Processor n stores to different addresses with same set, need eviction, also test snooping and writeback
      int set_shift = LG_CACHE_LINE_SIZE;
      int lg_num_sets = 3;
      int set_mask = (1 << lg_num_sets) - 1;
      int set = (addr >> set_shift) & set_mask;
      // Different addresses with same set, random address but with set bits replace back
      if (!response.retry_p && op_count < 4) {
        addr = (random() % test_args.addr_range) & ~(set_mask << set_shift); // Clear set bits
        addr |= (set << set_shift); // Apply the calculated set bits
        op_count++;
      } else if (!response.retry_p && op_count == 4) {
        addr = 324; // Fixed address
        op_count = 0;
      } 
      response = cache->store(addr, 0, cur_cycle, response.retry_p);
      if (args.verbose) {
        printf("Processor %d: Storing to address %d, data: %d\n", proc, addr, cur_cycle);
      }
    }
  break;

  case 3:
    // based on the proc id, we will do a store first and followed by a load, 
    // each store is privous proc's homesite address, 
    // and the load is the current proc's homesite address.
    
    // Store operation (store to the previous processor's home address)
      if (!store_done) { // Check if this proc hasn't completed store
        if (proc == 0) {
          addr_wr = (args.num_procs * 256) - 1;  // Special case for proc 0, write to the last address
        } else {
          addr_wr = (proc * 256) - 1;
        }
        response = cache->store(addr_wr, 0, proc, response.retry_p); // Store the proc ID

        if (!response.retry_p) {  // If the store completes, mark it as done
          store_done = true; // Set the bit for this proc
          if (args.verbose) {
            printf("Processor %d: Storing to address %d, data: %d\n", proc, addr_wr, proc);
          }
        }
      }

      // Check if all stores are done before proceeding to load
      if (store_done & (cur_cycle>100)) { // All store operations are done
        // Load operation (load from the current processor's home address)
        if (!load_done) { // Check if this proc hasn't completed load
          addr_rd = ((proc+1) * 256) - 1;
          response = cache->load(addr_rd, 0, &data, response.retry_p); // Load the next proc's ID
          if (!response.retry_p) {  // If the load completes, mark it as done
            load_done = 1; // Set the bit for this proc
            if (args.verbose) {
              printf("Processor %d: Loading from address %d, data: %d\n", proc, addr_rd, data);
              // error if the data is not the next proc's ID
              if (proc == (args.num_procs-1)){
                if (data != 0) {
                  ERROR("data is not the next proc's ID");
                }
              } else {
                if (data != (proc+1)) {
                  ERROR("data is not the next proc's ID");
                }
              }
            }
          }
        }
      }

    break;

    case 4:
      // int cur_op = program[proc][pc[proc]].op;
      // address_t cur_addr=program[proc][pc[proc]].addr;
      // int cur_data=program[proc][pc[proc]].data;
      
      // if(cur_op){
      //   response = cache->load(cur_addr, 0, &data, response.retry_p);
      // }
    break;

  default:
    ERROR("don't know this test case");
  }
}

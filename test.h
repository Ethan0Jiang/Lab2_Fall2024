// test.h
//   Derek Chiou
//     Oct. 8, 2023

// STUDENTS: YOU ARE EXPECTED TO PUT YOUR TESTS IN THIS FILE, ALONG WITH TEST.cpp

#include "types.h"

// models a processor's ld/st stream
class proc_t {
  int proc;
  response_t response;
  bool store_done;;
  bool load_done;
  bool initial_load_done; 
  bool final_load_done;

  address_t addr;
  address_t addr_rd;
  address_t addr_wr;
  int retry_count;
  int max_retry_count;
  bool ld_p;

  cache_t *cache;

 public:
  proc_t(int p);
  void init();
  void bind(cache_t *c);
  void advance_one_cycle();
  
};


void init_test();
void finish_test();

// ***** FYTD ***** 

typedef struct {
  int addr_range;
} test_args_t;


extern test_args_t test_args;

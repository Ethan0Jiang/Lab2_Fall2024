// test.h
//   Derek Chiou
//     Oct. 8, 2023

// STUDENTS: YOU ARE EXPECTED TO PUT YOUR TESTS IN THIS FILE, ALONG WITH TEST.cpp

#include "types.h"

// models a processor's ld/st stream
class proc_t {
  int proc;
  response_t response;

  address_t addr;
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

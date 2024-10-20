// helpers.h
//   by Derek Chiou
//      Oct. 8, 2023
// 

// STUDENTS: YOU ARE NOT ALLOWED TO MODIFY THIS FILE

#include <stdio.h>
#include <stdlib.h>

#include "generic_error.h"
#include "types.h"
#include "helpers.h"

extern uint gen_node_mask;
extern uint gen_local_addr_shift;
extern int lg_cache_line_size;

void terminate_on_error() {
  exit(1);
}

int gen_node(address_t addr) { 
  return((addr >> LG_INTERLEAVE_SIZE) & gen_node_mask);
}

int gen_local_addr(address_t addr) {
  int top = (addr >> gen_local_addr_shift) << LG_INTERLEAVE_SIZE;
  int bottom = (addr & ((1 << LG_INTERLEAVE_SIZE) - 1));
  
  return(top | bottom);
}

int gen_local_cache_line(address_t addr) {
  int laddr = gen_local_addr(addr);

  return(laddr >> LG_CACHE_LINE_SIZE);
}

bool within_tolerance(double a, double b, double t) {
  double diff = (a > b) ? (a - b) : (b - a);
  
  return(diff <= t);
}


void copy_cache_line(data_t dest, data_t src) {
  for (int i = 0; i < CACHE_LINE_SIZE; ++i) dest[i] = src[i];
}


int lg(int x) {
  int r = 0;

  while(x = (x >> 1)) {
    ++r;
  }
  return(r);
}

char *PRI2STRING(pri_t pri) {
  switch(pri) {
  case 0: return((char *)"PRI0");
  case 1: return((char *)"PRI1");
  case 2: return((char *)"PRI2");
  case 3: return((char *)"PRI3");
  default: ERROR("don't know this pri");
    return((char *)"ERROR");
  }
}



// types.h
//   by Derek Chiou
//      Oct. 6, 2023
// 

// STUDENTS: YOU ARE NOT ALLOWED TO MODIFY THIS FILE.
// for 382N-10

#ifndef TYPES_H
#define TYPES_H

const int ADDR_SIZE = 32;          // 32b address space
const int CACHE_LINE_SIZE = 8;     // number of words in cacheline,  needs to be consistent with LG_CACHE_LINE_SIZE
const int LG_CACHE_LINE_SIZE = 3;  // needs to be consistent with CACHE_LINE_SIZE
const int MEM_SIZE = 1024;
const int DIR_MEM_SIZE = 256;
const int NUM_PRIORITIES = 4;

const int NETWORK_LATENCY = 10;

typedef unsigned int uint;

typedef uint address_t;      // cannot be addr_t because cygwin defines
typedef uint address_tag_t;
// permit_tag_t is MESI state
//INVALID = 0, SHARED = 1, EXCLUSIVE = 2, MODIFIED = 3
typedef enum {INVALID, SHARED, EXCLUSIVE, MODIFIED} permit_tag_t; 
typedef enum {READ, WRITEBACK, INVALIDATE, NO_BUSOP} busop_t;
typedef enum {PRI0, PRI1, PRI2, PRI3} pri_t;
typedef int replacement_t;
typedef int bus_tag_t;

typedef int data_t[CACHE_LINE_SIZE];

typedef struct {
  int num_procs;
  int num_cycles;
  int test;
  int verbose;
} args_t;

extern args_t args;

// forward declarations
class iu_t;
class cache_t;
class network_t;

typedef struct {
  bool hit_p;  // hit or miss
  bool retry_p; // retry or not
} response_t;

typedef struct {
  busop_t busop;
  address_t addr;
  bus_tag_t tag;
  permit_tag_t permit_tag;
  data_t data;
} proc_cmd_t;

typedef struct {
  bool valid_p;
  int dest;
  int src;

  proc_cmd_t proc_cmd;
} net_cmd_t;

extern int cur_cycle;
extern int cache_line_size;


#endif

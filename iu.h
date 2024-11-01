// iu.h
//   by Derek Chiou
//      March 4, 2007
// 

// STUDENTS: YOU ARE ALLOWED TO MODIFY THIS FILE, BUT YOU SHOULDN'T NEED TO MODIFY MUCH, IF ANYTHING.
// for 382N-10

#ifndef IU_H
#define IU_H
#include "types.h"
#include "my_fifo.h"
#include "cache.h"
#include "network.h"

class iu_t {
  int node;

  int local_accesses;
  int global_accesses;

  typedef uint dir_t[CACHE_LINE_SIZE/4];  // CACHE_LINE_SIZE/4 = 2

  data_t mem[MEM_SIZE]; // data_t is an array of CACHE_LINE_SIZE, which is 8 int
  dir_t dir_mem[MEM_SIZE]; // dir_t is an array of 2 int, dir_mem has same # line as mem, 
                           // first int is one hot encoding of each processor,   // start compare from the LSB (node0), the [n-1, n-2, ...,1,0] bit represent the ownership of the data
                           // second int is the state of the line

  

  cache_t *cache;
  network_t *net;

  bool proc_cmd_p;
  proc_cmd_t proc_cmd;

  bool proc_cmd_writeback_p;
  proc_cmd_t proc_cmd_writeback;

  bool pri3_p;
  net_cmd_t pri3;
  bool pri2_p;
  net_cmd_t pri2;
  bool pri1_p;
  net_cmd_t pri1;
  bool pri0_p;
  net_cmd_t pri0;

  bool pri0_sent_p = false; // true means send.
  bool pri1_sent_p = false; // true means send.
  bool pri2_sent_p = false; // true means send. 
  bool pri3_sent_p = false; // true means send.

  int invalid_send_count = 0;
  bool invalid_sent_init = false;


  // processor side
  bool process_proc_request(proc_cmd_t proc_cmd);

  // network side
  bool process_net_request(net_cmd_t net_cmd);
  bool process_net_reply(net_cmd_t net_cmd);

 public:
  iu_t(int __node);

  void bind(cache_t *c, network_t *n);

  void advance_one_cycle();
  void print_stats();

  // processor side
  bool from_proc(proc_cmd_t pc);
  bool from_proc_writeback(proc_cmd_t pc);
  
  // network side
  bool from_net(net_cmd_t nc);
  
};
#endif

// iu.cpp
//   by Derek Chiou
//      March 4, 2007
//      modified Oct. 8, 2023
// 

// STUDENTS: YOU ARE EXPECTED TO MAKE MOST OF YOUR MODIFICATIONS IN THIS FILE.
// for 382N-10

#include "types.h"
#include "helpers.h"
#include "my_fifo.h"
#include "cache.h"
#include "iu.h"

// this is the constructor for the IU class
iu_t::iu_t(int __node) {
  node = __node;
  for (int i = 0; i < MEM_SIZE; ++i)
    for (int j = 0; j < CACHE_LINE_SIZE; ++j) // CACHE_LINE_SIZE is the number of words in cacheline
      // initialize memory to 0
      mem[i][j] = 0;

  proc_cmd_p = false;  // default is false
  proc_cmd_writeback_p = false; // this is a writeback request from the processor
  
}

// this is the constructor for the IU class, which connects the IU to the cache and network
void iu_t::bind(cache_t *c, network_t *n) {
  cache = c;
  net = n;
}

// this method advances the IU by one cycle
void iu_t::advance_one_cycle() {
  if (net->from_net_p(node, PRI0)) {  //PRI0 is the highest priority
    //network’s acknowledgment or response to a request that the IU (or a processor) previously sent. 
    // Replies indicate that the requested operation (e.g., a read, write, or invalidation) has been completed, 
    // and they typically contain data or status information.
    process_net_reply(net->from_net(node, PRI0));

  } else if (net->from_net_p(node, PRI1)) { // from_net_p is a boolean that checks if there is a request from the network to the IU
    process_net_request(net->from_net(node, PRI1)); 

  } else if (net->from_net_p(node, PRI2)) {
    process_net_request(net->from_net(node, PRI2));

  } else if (net->from_net_p(node, PRI3)) {
    process_net_request(net->from_net(node, PRI3));

  } else if (proc_cmd_writeback_p) { // proc_cmd_writeback_p is a boolean that checks if there is a writeback request from the processor to the IU?
    if (process_proc_request(proc_cmd_writeback)) { // proc_cmd_writeback is a struct (proc_cmd_t) that contains the address, data, and busop, which defined in types.h
      proc_cmd_writeback_p = false;
    }
  } else if (proc_cmd_p) { // in privous cycle, set proc_cmd_p to true, with it proc_cmd, in this cycles, we process the request, and set proc_cmd_p to false if the request is completed
    if (!process_proc_request(proc_cmd)) {  // process_proc_request return false means the request is completed, true means the request is not completed (e.g., the request is sent to the network)
      proc_cmd_p = false;
    }
  }    // ???snoop here??? would here causing dead lock?
}

// processor side

// this interface method buffers a non-writeback request from the processor, returns true if cannot complete
// ??? why we need to distingush non-writeback request and writeback request
bool iu_t::from_proc(proc_cmd_t pc) {
  if (!proc_cmd_p) {
    proc_cmd_p = true; // mean there is a request from the processor to the IU
    proc_cmd = pc; // set the proc_cmd to the pc, where pc is  a struct (proc_cmd_t) that contains the address, data, and busop

    return(false);
  } else {
    return(true); // we do not want to return true, hence proc_cmd_p need to be false
  }
}

// this interface method buffers a writeback request from the processor, returns true if cannot complete
bool iu_t::from_proc_writeback(proc_cmd_t pc) {  // pc means processor command, such as WRITEBACK, READ, INVALIDATE
  if (!proc_cmd_writeback_p) { // checks if there is a writeback request from the processor to the IU
    proc_cmd_writeback_p = true;
    proc_cmd_writeback = pc;
    // start the writeback process using address, data, and busop???

    return(false);
  } else {
    return(true);
  }
}

bool iu_t::process_proc_request(proc_cmd_t pc) {
  int dest = gen_node(pc.addr);
  int lcl = gen_local_cache_line(pc.addr);

  NOTE_ARGS(("%d: addr = %d, dest = %d", node, pc.addr, dest));

  if (dest == node) { // local

    ++local_accesses;
    proc_cmd_p = false; // clear proc_cmd
    
    // for example, if doing a INVALIDATE to 
    switch(pc.busop) {
    case READ:
      // if it is a read, should we check the current tag state of the cache line first from the home site?
      // if the tag state is exclusive(means could be modified), we need other to writeback first
      // if the tag state is shared, we can directly read the data from the local cache, but if we are doing RWITM, we need to invalidate other nodes (send net request)
      // if the tag state is invalid, we can directly read the data from the local cache
      copy_cache_line(pc.data, mem[lcl]); // dest, src: copy data from local to a load buffer, which will write to cache using reply
      // here should ask home site to invalidate other cache lines if pc state is MODIFIED

      cache->reply(pc);
      return(false);
      
    case WRITEBACK:
      copy_cache_line(mem[lcl], pc.data);
      return(false);
      
    case INVALIDATE:
      // ***** FYTD *****
      return(false);  // need to return something for now
      break;
    }
    
  } else { // global
    ++global_accesses;
    net_cmd_t net_cmd;

    net_cmd.src = node;  // node is the one sending the request
    net_cmd.dest = dest;
    net_cmd.proc_cmd = pc;

    // for the WB case, return TRUE if goes to network successfully, not need to retry
    // for the other cases, return TRUE if goes to network successfully, need to retry until success?
    return(net->to_net(node, PRI1, net_cmd)); // return true if goes to network successfully
  }
  return(false); // need to return something
}


// receive a net request
bool iu_t::process_net_request(net_cmd_t net_cmd) {  // maybe this will call the snoop? 
  proc_cmd_t pc = net_cmd.proc_cmd;

  int lcl = gen_local_cache_line(pc.addr);
  int src = net_cmd.src;
  int dest = net_cmd.dest;

  // ***** FYTD *****
  // sanity check
  if (gen_node(pc.addr) != node) 
    ERROR("sent to wrong home site!"); 

  switch(pc.busop) {
  case READ: // assume local     
    net_cmd.dest = src;
    net_cmd.src = dest;
    copy_cache_line(pc.data, mem[lcl]);
    net_cmd.proc_cmd = pc;


    return(net->to_net(node, PRI0, net_cmd));
      
  case WRITEBACK:
    copy_cache_line(mem[lcl], pc.data);  // other node trying to writeback data to the home site (Im the home site)
    return(false);
      
  case INVALIDATE: 
  default:
    // ***** FYTD *****
    return(false);  // need to return something for now
  }
}


bool iu_t::process_net_reply(net_cmd_t net_cmd) {    // this is a reply from the network that set proc_cmd_p back to false
  proc_cmd_t pc = net_cmd.proc_cmd;

  // ***** FYTD *****

  proc_cmd_p = false; // clear out request that this reply is a reply to

  switch(pc.busop) {
  case READ: // assume local
    cache->reply(pc);
    return(false);
      
  case WRITEBACK:
  case INVALIDATE:
    ERROR("should not have gotten a reply back from a write or an invalidate, since we are incoherent");
    return(false);  // need to return something for now
  default:
    ERROR("should not reach default");
    return(false);  // need to return something
  }
}

void iu_t::print_stats() {
  printf("------------------------------\n");
  printf("%d: iu\n", node);
  
  printf("num local  accesses = %d\n", local_accesses);
  printf("num global accesses = %d\n", global_accesses);
}

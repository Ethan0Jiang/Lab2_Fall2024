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

  // initialize the directory memory to INVALID
  for (int i = 0; i < MEM_SIZE; ++i) {
    dir_mem[i][0] = 0;
    dir_mem[i][1] = INVALID;
  }


  proc_cmd_p = false;  // default is false
  proc_cmd_writeback_p = false; // this is a writeback request from the processor
  pri3_p = false; 
  pri2_p = false;
  pri1_p = false;
  pri0_p = false;
  
}

// this is the constructor for the IU class, which connects the IU to the cache and network
void iu_t::bind(cache_t *c, network_t *n) {
  cache = c;
  net = n;
}

// this method advances the IU by one cycle
void iu_t::advance_one_cycle() {
  if (!pri0_p) { // true if there is no request on hold, create a pri0
    pri0_p = true;
    pri0 = net->from_net(node, PRI0); // get the request from the network
  } else if (pri0_p) { // true if there is a request on hold
    if (!process_net_reply(pri0)) {
      pri0_p = false;
    }

  } else if (!pri1_p) { // true if there is no request on hold, create a pri1
    pri1_p = true;
    pri1 = net->from_net(node, PRI1); // get the request from the network
  } else if (pri1_p) { // true if there is a request on hold
    if (!process_net_request(pri1)) {
      pri1_p = false;
    }

  } else if (!pri2_p) {
    pri2_p = true;
    pri2 = net->from_net(node, PRI2);
  } else if (pri2_p) {
    if (!process_net_request(pri2)) {
      pri2_p = false;
    }

  } else if (!pri3_p) {
    pri3_p = true;
    pri3 = net->from_net(node, PRI3);
  } else if (pri3_p) {
    if (!process_net_request(pri3)) {
      pri3_p = false;
    }
  // } else if (proc_cmd_writeback_p) { // proc_cmd_writeback_p is a boolean that checks if there is a writeback request from the processor to the IU?
  //   if (process_proc_request(proc_cmd_writeback)) { // proc_cmd_writeback is a struct (proc_cmd_t) that contains the address, data, and busop, which defined in types.h
  //     proc_cmd_writeback_p = false;
  //   }

  } else if (proc_cmd_p) { // in privous cycle, set proc_cmd_p to true, with it proc_cmd, in this cycles, we process the request, and set proc_cmd_p to false if the request is completed
    if (!process_proc_request(proc_cmd)) {  // process_proc_request return false means the request is completed, true means the request is not completed (e.g., the request is sent to the network)
      proc_cmd_p = false;
    }
  }
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
    
    switch(pc.busop) {
    case READ:
      // if it is a read, should we check the current tag state of the cache line first from the home site?
      // if the tag state is exclusive(means could be modified) in other node, we need other to writeback first
      // if the tag state is shared, we can directly read the data from the local cache, but if we are doing RWITM, we need to invalidate other nodes (send net request before change the data)
      // if the tag state is invalid, we can directly read the data from the local cache
      copy_cache_line(pc.data, mem[lcl]); // dest, src
      // here should ask home site to invalidate other cache lines if pc state is MODIFIED
      if (pc.permit_tag == MODIFIED) {  // RWITM
        // for each nodes own the data, we need to send a INVALIDATE request to them
        if(dir_mem[lcl][1] == SHARED) { // if the state is SHARED, we need to send a INVALIDATE request to the node 
          //this line check bitwise or of dir_mem[lcl][0] is equal to 1 << node
          if (dir_mem[lcl][0] != int(1 << node)) {  // this check if all other nodes invalidate the data
            for (int i = 0; i < 32; ++i) {
              if (dir_mem[lcl][0] & (1 << i)) { // start compare from the LSB (node0), the [n-1, n-2, ...,1,0] bit represent the ownership of the data
                net_cmd_t net_cmd;
                net_cmd.src = node;
                net_cmd.dest = i;
                net_cmd.proc_cmd = (proc_cmd_t){INVALIDATE, pc.addr, 0, INVALID};  // {busop, addr, tag, permit_tag} homesite send invalidation request to the node who has the data
                net->to_net(node, PRI1, net_cmd); // PRI1: homesite send request to remote
              }
            return(true); // need to retry
            }
          } else {  // all other nodes data are invalidated, now we can change the data to MODIFIED in cache, no retry
            // cache->reply(pc);  no need this line, since the node has the updated data
            dir_mem[lcl][1] = EXCLUSIVE; // change the state in dir_mem to EXCLUSIVE ???  
            return(false);
          } 
        }else if(dir_mem[lcl][1] == EXCLUSIVE) { // if the state is EXCLUSIVE (Modified)
          // sanity check in dir_mem, that current node is the only owner of the data  !!! some one
          if (dir_mem[lcl][0] != int(1 << node)) { ERROR("more than one node has the data"); }
          return(false);
        }else if(dir_mem[lcl][1] == INVALID) { // no one has the data
          dir_mem[lcl][0] = 1 << node; // set the ownership to the current node
          dir_mem[lcl][1] = EXCLUSIVE; // set the state to EXCLUSIVE

          // write the data from MEM to cache
          cache->reply(pc);
          return(false);
        }
            
          
      }

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
    // first check the dir_mem to see the state of the cache line
    // if the state is Exclusive, we need other to writeback first



    copy_cache_line(pc.data, mem[lcl]); // copy data from local to a load buffer, which will write to cache using reply
    net_cmd.proc_cmd = pc;


    return(net->to_net(node, PRI0, net_cmd));  // PROI0 means net reply
      
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

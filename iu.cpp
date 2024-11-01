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
    if (!process_proc_request(proc_cmd)) {  // process_proc_request return false means the request is completed, true means the request is not completed
      proc_cmd_p = false;
    }
  }
}

// processor side
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
    return(false);
  } else {
    return(true);
  }
}

bool iu_t::process_proc_request(proc_cmd_t pc) {
  int dest = gen_node(pc.addr);
  int lcl = gen_local_cache_line(pc.addr);

  NOTE_ARGS(("%d: addr = %d, dest = %d", node, pc.addr, dest));
  pc.tag = -1; // bus_tag == -1 means local access !!check the default value of pc.tag!!
  response_t snoop_p = (cache->snoop(pc)); // snoop the data in the cache, the return here not matter
  //extra logic to process local writeback
  if(proc_cmd_writeback_p){
    if(gen_node(proc_cmd_writeback.addr) == node){ //local writeback 
      copy_cache_line(mem[lcl], proc_cmd_writeback.data);//do local writeback, from cache to memory
      proc_cmd_writeback_p = false;//clear proc_cmd_writeback_p    
    }
    else{//network writeback, put into net request (PRI1)
      proc_cmd_writeback.tag = 1;
      net_cmd_t net_cmd;
      net_cmd.src = node;
      net_cmd.dest = gen_node(proc_cmd_writeback.addr);
      net_cmd.proc_cmd = proc_cmd_writeback;
      if(net->to_net(node, PRI1, net_cmd)){ // return a true is put into the networke
        //clear the local writeback buffer because the writeback is committed to network
        proc_cmd_writeback_p = false;
        return true; //need to retry the process_proc_request to move on
      }
      else{
        return true; //need to retry the process_proc_request to hopefully put writeback on network
      }
    }
  }

  if (dest == node) { // local

    ++local_accesses;  // local access increase when hit or not???
    // proc_cmd_p = false; // clear proc_cmd, not guarantee that the request is completed
    
    switch(pc.busop) {
    case READ:    
    // READ only
    if (pc.permit_tag == SHARED || pc.permit_tag == EXCLUSIVE) { // cache is read only
      // check the DIR_MEM to the state of the cache line
      if (dir_mem[lcl][1] == INVALID){ // no one has the data, but we may need envict data out
      // to do the eviction, we first snoop the data, and evict the data to proc_cmd_writeback if needed
      // if need eviction needed snoop return true, with proc_cmd_writeback filled with infos
      // and then do the local writeback or generate a net_request (PRI1) to the network
      // and then this function return true

      // if no eviction needed, snoop return false, and we can call reply to read data into the cache
        dir_mem[lcl][1] = EXCLUSIVE;
        dir_mem[lcl][0] = 1 << node;
        
        proc_cmd_t temp = (proc_cmd_t){pc.busop, pc.addr, 0, EXCLUSIVE};
        copy_cache_line(pc.data, mem[lcl]);
        cache->reply(temp);
        return(false);
      }
      else if (dir_mem[lcl][1] == SHARED){
        dir_mem[lcl][0] |= 1 << node;
        proc_cmd_t temp = (proc_cmd_t){pc.busop, pc.addr, 0, SHARED};
        copy_cache_line(pc.data, mem[lcl]);
        cache->reply(temp);
        return(false);
      }
      else if (dir_mem[lcl][1] == EXCLUSIVE){
        // This request needs to be put into the PRI3 buffer at this point
        // in PRI3, we send a prority 2 request as the confirmation, make sure only send once

        //Generate secondary request
        proc_cmd_t temp = (proc_cmd_t){pc.busop, pc.addr, 2, SHARED};//READ busop, requesting address, priority 2, SHARED (other guy go shared, sharing is caring), data is not needed
        net_cmd_t net_cmd;
        net_cmd.dest = gen_node(pc.addr);
        net_cmd.src  = node;
        net_cmd.proc_cmd = temp;

        //Attempt sending secondary request
        if(net->to_net(net_cmd.dest,PRI2,net_cmd)){
          pri2_sent_p = true; // in PRI3, we need to recheck those flags  
        }
        else{ //failed, promoted P3 request will retry
          pri2_sent_p = false;
        }

        //promote here (LETS GOOOO I EARN 2% MORE SALARY NOW FOR DOUBLE THE WORK!!!!)
        pri3_p = true;
        proc_cmd_t temp_P3 = (proc_cmd_t){pc.busop, pc.addr, 3, EXCLUSIVE}; //Need to generate confirmation request for Read Miss
        pri3.dest = node;
        pri3.src  = node;
        pri3.proc_cmd = temp_P3;
      }
      else{
        ERROR("Modified or non-ESI state in directory entry not allowed.");
      }
    } 
    else if(pc.permit_tag == MODIFIED){ //RWITM
      if (dir_mem[lcl][1] == INVALID){
        dir_mem[lcl][1] = EXCLUSIVE;
        dir_mem[lcl][0] = 1 << node;
        
        proc_cmd_t temp = (proc_cmd_t){pc.busop, pc.addr, 0, MODIFIED}; //READ, addr, 0, MODIFIED
        copy_cache_line(pc.data, mem[lcl]);
        cache->reply(temp);
        return(false);
      }
      else if (dir_mem[lcl][1] == SHARED){ //generate invalidations, multicast or broadcast
        //implement invalidation issuing retry buffer
        if(!invalid_sent_init){
          invalid_send_count = dir_mem[lcl][0];
          invalid_sent_init = true;
        }

        for(int i = 0; i < 32; i++){
          // for each 32 node, we generate a invalidation request to that node
          // check the bit on invalid_send_count, if it is 1, we generate a invalidation request
          if(invalid_send_count & 1 << i){
            proc_cmd_t temp = (proc_cmd_t){INVALIDATE, pc.addr, 0, SHARED};
            net_cmd_t net_cmd;
            net_cmd.dest = i;
            net_cmd.src  = node;
            net_cmd.proc_cmd = temp;
            if(net->to_net(i, PRI2, net_cmd)){
              invalid_send_count &= ~(1 << i); // clear the bit
            }
          }
        } //this will never be retried in this branch, but may be retried in the proc_net_request(PRI3) branch.

        // promote to PRI3
        pri3_p = true;
        proc_cmd_t temp_P3 = (proc_cmd_t){pc.busop, pc.addr, 3, MODIFIED}; // need to generate invalidations for RWITM
        pri3.dest = node;
        pri3.src  = node;
        pri3.proc_cmd = temp_P3;
      }
      else if (dir_mem[lcl][1] == EXCLUSIVE){ // do  E->I->E
        // generate an invalidation request to the owner of the data
        net_cmd_t invalidate_cmd;
        // in dir_mem, who has the data
        for(int i=0; i<32; i++){
          if(dir_mem[lcl][0] == i<<1){
            invalidate_cmd.dest=i;
            break;
          }
        }
        invalidate_cmd.src  = node;
        proc_cmd_t temp = (proc_cmd_t){INVALIDATE, pc.addr, 2, EXCLUSIVE}; 
        invalidate_cmd.proc_cmd = temp;
        if(net->to_net(invalidate_cmd.dest, PRI2, invalidate_cmd)){
          pri2_sent_p = true;
        }
        else{
          pri2_sent_p = false;
        }
        
        // promote to PRI3
        pri3_p = true;
        proc_cmd_t temp_P3 = (proc_cmd_t){pc.busop, pc.addr, 3, MODIFIED};
        pri3.dest = node;
        pri3.src  = node;
        pri3.proc_cmd = temp_P3;   // TODO: Review everything and finish up base implementation !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      }
      else{
        ERROR("Modified or non-ESI state in directory entry is not allowed.");
      }
    }
    else{
      ERROR("should not request READ to INVALID state");
    }

      
    // case WRITEBACK: // maybe this case will not be used
    //   copy_cache_line(mem[lcl], pc.data);
    //   return(false);
      
    // case INVALIDATE:
    //   // ***** FYTD *****
    //   return(false);  // need to return something for now
    //   break;
    }
    
  
  } else { // global
    ++global_accesses;

    switch(pc.busop) {
    case READ:    
    // READ only
    if (pc.permit_tag == SHARED || pc.permit_tag == EXCLUSIVE) { // cache is read only
      // check the DIR_MEM to the state of the cache line
      if (!pri3_sent_p){
        //Generate a PRI3 request
        net_cmd_t net_cmd_P3;
        net_cmd_P3.dest = dest;
        net_cmd_P3.src  = node;
        proc_cmd_t temp_P3 = (proc_cmd_t){pc.busop, pc.addr, 3, pc.permit_tag}; // sent permit_tag but receive the updated permit_tag from PRIO0
        net_cmd_P3.proc_cmd = temp_P3;
        pri3_sent_p = net->to_net(node, PRI3, net_cmd_P3);
      }
      return(true);
    }
    else if (pc.permit_tag == MODIFIED){ //RWITM

    }
      

    net_cmd_t net_cmd;

    net_cmd.src = node;  // node is the one sending the request
    net_cmd.dest = dest;
    net_cmd.proc_cmd = pc;

    // for the WB case, return TRUE if goes to network successfully, not need to retry
    // for the other cases, return TRUE if goes to network successfully, need to retry until success?
    return(net->to_net(node, PRI1, net_cmd)); // return true if goes to network successfully
  }

  return(false); // need to return something by default
}


// receive a net request
bool iu_t::process_net_request(net_cmd_t net_cmd) { 
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

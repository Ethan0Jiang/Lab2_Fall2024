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
void iu_t::advance_one_cycle() { //maybe add writeback cache if time

  //Dequeuing into buffer can happen concurrently
  if(!pri0_p && net->from_net_p(node, PRI0)){ //buffer free, something in network queue
    pri0_p = true;
    pri0 = net->from_net(node, PRI0);
  }
  
  if(!pri1_p && net->from_net_p(node, PRI1)){
    pri1_p = true;
    pri1 = net->from_net(node, PRI1);
  }
  
  if(!pri2_p && net->from_net_p(node, PRI2)){
    pri2_p = true;
    pri2 = net->from_net(node, PRI2);
  }

  if(!pri3_p && net->from_net_p(node, PRI3)){
    pri3_p = true;
    pri3 = net->from_net(node, PRI3);
  }

  //Process buffers
  if (pri0_p) { // true if there is a request on hold
    if (!process_net_reply(pri0)) {
      pri0_p = false;
    }
  } else if (pri1_p) { // true if there is a request on hold
    if (!process_net_request(pri1)) {
      pri1_p = false;
    }
  } else if (pri2_p) {
    if (!process_net_request(pri2)) {
      pri2_p = false;
    }
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

bool iu_t::from_proc_writeback_PRI2(proc_cmd_t pc) { 
  if (!proc_cmd_writeback_p_PRI2) {
    proc_cmd_writeback_p_PRI2 = true;
    proc_cmd_writeback_PRI2 = pc;
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
      if(net->to_net(node, PRI1, net_cmd)){ // return a true is put into the network
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
    if (pc.permit_tag == SHARED) { // cache is read only
      // check the DIR_MEM to the state of the cache line
      if (dir_mem[lcl][1] == INVALID){ // no one has the data, but we may need envict data out
      // to do the eviction, we first snoop the data, and evict the data to proc_cmd_writeback if needed
      // if need eviction needed snoop return true, with proc_cmd_writeback filled with infos
      // and then do the local writeback or generate a net_request (PRI1) to the network
      // and then this function return true

        dir_mem[lcl][1] = EXCLUSIVE;
        dir_mem[lcl][0] = 1 << node; // 1 << 0 is 00000001, 1 << 1 is 00000010, 1 << 2 is 00000100, 1 << 3 is 00001000
        
        proc_cmd_t temp = (proc_cmd_t){pc.busop, pc.addr, 0, EXCLUSIVE}; // set requesting node cache to exclusive
        copy_cache_line(temp.data, mem[lcl]);
        cache->reply(temp);
        return(false);
      }
      else if (dir_mem[lcl][1] == SHARED){
        dir_mem[lcl][0] |= 1 << node;
        // In PROI0 commands, permit_tag means the state that remote node cache need to go to
        proc_cmd_t temp = (proc_cmd_t){pc.busop, pc.addr, 0, SHARED}; // set requesting node cache to shared
        copy_cache_line(temp.data, mem[lcl]);
        cache->reply(temp);
        return(false);
      }
      else if (dir_mem[lcl][1] == EXCLUSIVE){
        // This request needs to be put into the PRI3 buffer at this point
        // in PRI3, we send a prority 2 request as the confirmation, make sure only send once

        //Generate secondary request
        // in PROI2 commands, permit_tag means the state at the HOME site
        proc_cmd_t temp = (proc_cmd_t){pc.busop, pc.addr, 2, EXCLUSIVE};//READ busop, requesting address, priority 2, data is not needed
        net_cmd_t net_cmd;
        for(int i=0; i<32; i++){
          if(dir_mem[lcl][0] == (1 << i)){
            net_cmd.dest=i;
            break;
          }
        }
        net_cmd.src  = node;
        net_cmd.proc_cmd = temp;


        // IMP: this could be buggy, how can we tell this pri2_sent_p is for this request or another request, when to reset this flag?
        // IMP: reset pri2_sent_p to false when process_net_request(PRIO3) is killed
        //Attempt sending secondary request
        if(net->to_net(node,PRI2,net_cmd)){ // IMP: the first argument is the src node!
          pri2_sent_p = true; 
        }
        else{ //failed, promoted P3 request will retry
          pri2_sent_p = false; // by default is false
        }

        //promote here (LETS GOOOO I EARN 2% MORE SALARY NOW FOR DOUBLE THE WORK!!!!)
        pri3_p = true;
        // in PROI3 commands, permit_tag is the state receive from cache (least request state)
        proc_cmd_t temp_P3 = (proc_cmd_t){pc.busop, pc.addr, 3, SHARED}; //Need to generate confirmation request for Read Miss
        pri3.dest = node;
        pri3.src  = node;
        pri3.proc_cmd = temp_P3;
        return true;
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
        copy_cache_line(temp.data, mem[lcl]);
        cache->reply(temp);
        return(false);
      }
      else if (dir_mem[lcl][1] == SHARED){ //generate invalidations, multicast or broadcast
        //implement invalidation issuing retry buffer
        invalid_send_count = dir_mem[lcl][0];
        invalid_send_init = 1;


        /// !!create a function for this part!!
        for(int i = 0; i < 32; i++){
          // for each 32 node, we generate a invalidation request to that node
          // check the bit on invalid_send_count, if it is 1, we generate a invalidation request (PRIO2)
          if(invalid_send_count & (1 << i)){
            // TODO: handle local case if homesite is remote node
            proc_cmd_t temp = (proc_cmd_t){INVALIDATE, pc.addr, 2, SHARED}; // for PROI2, permit_tag is the state at HOME site
            net_cmd_t net_cmd;
            net_cmd.dest = i;
            net_cmd.src  = node;
            net_cmd.proc_cmd = temp;
            // IMP: the first argument should be the src node!
            if(net->to_net(node, PRI2, net_cmd)){
              invalid_send_count &= ~(1 << i); // clear the bit 
            }        
          }
        } //this will never be retried in this branch, but may be retried in the proc_net_request(PRI3) branch.

        if (invalid_send_count == 0){ // if all the invalidation request is sent
          pri2_sent_p = true;
        } else {
          pri2_sent_p = false;
        }

        // promote to PRI3
        pri3_p = true;
        proc_cmd_t temp_P3 = (proc_cmd_t){pc.busop, pc.addr, 3, MODIFIED}; // need to generate invalidations for RWITM
        pri3.dest = node;
        pri3.src  = node;
        pri3.proc_cmd = temp_P3;

        return true;
      }
      else if (dir_mem[lcl][1] == EXCLUSIVE){ // do  E->I->E
        // generate an invalidation request to the owner of the data
        net_cmd_t invalidate_cmd;
        // in dir_mem, who has the data
        for(int i=0; i<32; i++){
          if(dir_mem[lcl][0] == (1 << i)){
            invalidate_cmd.dest=i;
            break;
          }
        }
        invalidate_cmd.src  = node;
        proc_cmd_t temp = (proc_cmd_t){INVALIDATE, pc.addr, 2, EXCLUSIVE}; 
        invalidate_cmd.proc_cmd = temp;

        if(net->to_net(node, PRI2, invalidate_cmd)){ 
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
        pri3.proc_cmd = temp_P3;
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
    } // end of switch
  } else { // global
    ++global_accesses;

    switch(pc.busop) {
      case READ:    
      // READ only
        if (pc.permit_tag == SHARED) { // cache is read only
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
      default: 
        ERROR("Processor should have issued only Read miss or RWITM");
    }
  }
  return(true); // need to return something by default
}


// receive a net request
bool iu_t::process_net_request(net_cmd_t net_cmd) { 
  
  proc_cmd_t pc = net_cmd.proc_cmd;
  // ***** FYTD *****
  // sanity check
  if (gen_node(pc.addr) != node) 
    ERROR("sent to wrong home site!"); 

  int lcl = gen_local_cache_line(pc.addr);
  int src = net_cmd.src;
  int dest = net_cmd.dest; //dest = node

  switch(pc.tag) {

    // case 0:
    //   ERROR("should not have gotten a PRI0 request in net request");
    
    case 1: 
      if (pc.busop == READ) { // READ means not a eviction, src node still have the data
        if (pc.permit_tag == EXCLUSIVE){ // the data maintains the same
          dir_mem[lcl][1] = SHARED;
          dir_mem[lcl][0] = dir_mem[lcl][0]; //don't change
          return false;
        }
        else if (pc.permit_tag == MODIFIED){
          dir_mem[lcl][1] = SHARED;
          copy_cache_line(mem[lcl], pc.data);
          return false;
        }
        else {
          ERROR("For PRI1, READ's permit_tag should be EXCLUSIVE or MODIFIED");
        }
      }
      else if (pc.busop == WRITEBACK) { // WRITEBACK means regular eviction, other src node do not have data anymore
        if (pc.permit_tag == EXCLUSIVE){
          dir_mem[lcl][1] = INVALID;
          dir_mem[lcl][0] = 0;
          return false;
        }
        else if (pc.permit_tag == MODIFIED){
          dir_mem[lcl][1] = INVALID;
          dir_mem[lcl][0] = 0;
          copy_cache_line(mem[lcl], pc.data);
          return false;
        } 
        else if (pc.permit_tag == SHARED){
          dir_mem[lcl][0] &= ~(1 << src);
          if (dir_mem[lcl][0] == 0){
            dir_mem[lcl][1] = INVALID;
          }
          return false;
        }
        else {
          ERROR("For PRI1, WRITEBACK's permit_tag should be EXCLUSIVE or MODIFIED or SHARED");
        }
    
      } 
      else if (pc.busop == INVALIDATE) { // INVALIDATE means coherent eviction, other src node do not have data anymore
        if (pc.permit_tag == EXCLUSIVE){
          dir_mem[lcl][1] = INVALID;
          dir_mem[lcl][0] = 0;
          return false;
        }
        else if (pc.permit_tag == MODIFIED){
          dir_mem[lcl][1] = INVALID;
          dir_mem[lcl][0] = 0;
          copy_cache_line(mem[lcl], pc.data);
          return false;
        } 
        else if (pc.permit_tag == SHARED){
          dir_mem[lcl][0] &= ~(1 << src);
          if (dir_mem[lcl][0] == 0){
            dir_mem[lcl][1] = INVALID;
          }
          return false;
        }
        else {
          ERROR("For PRI1, INVALIDATE's permit_tag should be EXCLUSIVE or MODIFIED or SHARED");
        }
      } else {
        ERROR("should not have gotten a PRI1 request busop other than READ or WRITEBACK or INVALIDATE");
      }

    case 2: // for PRI2, permit_tag is the state at HOME site
      if (pc.busop == READ) { // case change local nodes from E->S, or ask for WB if local node is in M
        if (pc.permit_tag == EXCLUSIVE){
          // EXCLUSIVE is the permit_tag from the homesite, we change it to SHARED if snooped in cache
          // Do nothing if it is INVALID in cache
          if(!proc_cmd_writeback_p_PRI2)
            response_t YOYOYO = cache->snoop(pc);

          if(proc_cmd_writeback_p_PRI2){
            net_cmd_t reply_p1;
            reply_p1.dest = src;
            reply_p1.src  = node;
            reply_p1.proc_cmd = proc_cmd_writeback_PRI2;
            if(net->to_net(node,PRI1,reply_p1)){
              proc_cmd_writeback_p_PRI2 = false;
              return false;
            }
            return true;
          }          
          return false;
        }
        else{
          ERROR("For PRI2, READ's permit_tag should be EXCLUSIVE");
        }
      }
      else if (pc.busop == INVALIDATE) { // case when Homesite doing invalidation, change local nodes to I
        if (pc.permit_tag == SHARED) {
          // SHARED is the permit_tag from the homesite
          if(!proc_cmd_writeback_p_PRI2)
            response_t DerekIsHotAF = cache->snoop(pc);
            
          if (proc_cmd_writeback_p_PRI2){
            net_cmd_t reply_p1;
            reply_p1.dest = src;
            reply_p1.src = node;
            reply_p1.proc_cmd = proc_cmd_writeback_PRI2;
            if (net->to_net(node, PRI1, reply_p1)){
              proc_cmd_writeback_p_PRI2 = false;
              return false; // means request completed
            }
            return true; // means request not completed
          }
          return false;
        }
        else if (pc.permit_tag == EXCLUSIVE){
          // EXCLUSIVE at the homesite, but could be modified at the local node
          if(!proc_cmd_writeback_p_PRI2)
            response_t CHIOUCHIOUUUUUBUYMEACAR = cache->snoop(pc);
            
          if (proc_cmd_writeback_p_PRI2){
            net_cmd_t reply_p1;
            reply_p1.dest = src;
            reply_p1.src = node;
            reply_p1.proc_cmd = proc_cmd_writeback_PRI2;
            if (net->to_net(node, PRI1, reply_p1)){
              proc_cmd_writeback_p_PRI2 = false;
              return false; // means request completed
            }
            return true; // means request not completed
          }
          return false;
        }
        else {
          ERROR("For PRI2, INVALIDATE's permit_tag should be SHARED");
        }
        
          

      } else {
        ERROR("should not have gotten a PRI2 request busop other than READ or INVALIDATE");
      }
    

    case 3: // PRI3
      if (pc.busop == READ) {
        if (pc.permit_tag == SHARED) { //READ MISS
          if (dir_mem[lcl][1]==INVALID){
            dir_mem[lcl][1] = EXCLUSIVE;
            dir_mem[lcl][0] = 1 << src;

            // generating a PRI0 request to the src node, with the updated permit_tag and data
            net_cmd_t reply_p0;
            reply_p0.dest = src;
            reply_p0.src = node;
            proc_cmd_t temp = (proc_cmd_t){pc.busop, pc.addr, 0, EXCLUSIVE};
            copy_cache_line(temp.data, mem[lcl]);
            reply_p0.proc_cmd = temp;
            
            // try to send the request to the network
            net->to_net(node, PRI0, reply_p0);
            pri2_sent_p = false;
            return false;
          }
          else if(dir_mem[lcl][1]==SHARED){
            //TODO: need to check if requesting node = homesite directory exclusive case
            //      and correctly manage buffer clears and local READ miss reply
            //      potential optimization. For now send request to yourself

            // maintains at SHARED
            dir_mem[lcl][0] |= 1 << src;

            // generate a PRI0 net request to src node with updated permit_tag and data
            net_cmd_t reply_p0;
            reply_p0.dest = src;
            reply_p0.src = node;
            proc_cmd_t temp = (proc_cmd_t){pc.busop, pc.addr, 0, SHARED};  // node in SHARED state
            copy_cache_line(temp.data, mem[lcl]);
            reply_p0.proc_cmd = temp;

            net->to_net(node, PRI0, reply_p0);
            pri2_sent_p = false;
            return (false);
          }
          else if(dir_mem[lcl][1]==EXCLUSIVE){ // check if the data been modified, then need update dir_mem
            
            proc_cmd_t temp = (proc_cmd_t){pc.busop, pc.addr, 2, EXCLUSIVE}; // for PRI2, permit_tag is the state at HOME site
            net_cmd_t net_cmd;

            for(int i=0; i<32; i++){
              if(dir_mem[lcl][0] == (1 << i)){
                // if (i == node){
                //   // local node is homesite
                //   // need to check if the data is modified locally in cache
                //   response_t snoop_p = (cache->snoop(temp)); // temp is the PRIO2 request sent, but do it locally, update the cache state
                //   // this would return some data in 
                //   if (proc_cmd_writeback_p_PRI2){ // there are some data to writeback in local memory
                //     copy_cache_line(mem[lcl], proc_cmd_writeback_PRI2.data); // writeback the data to memory
                //     proc_cmd_writeback_p_PRI2 = false; // clear the writeback buffer
                //     dir_mem[lcl][1] = SHARED; // update the state of the cache line
                //     dir_mem[lcl][0] = 1 << node; // update the ownership of the cache line
                //   }
                // }
                net_cmd.dest = i;
                break;
              }
            }
            
            net_cmd.src = node;
            net_cmd.proc_cmd = temp;

            if (!pri2_sent_p){
              if(net->to_net(node, PRI2, net_cmd)){
                pri2_sent_p = true;
              }
            }
            return true;
          }

        }
        else if (pc.permit_tag == MODIFIED) { //RWITM
          if(dir_mem[lcl][1]==INVALID){ //Change homesite to exclusive and reply requesting node with data
            invalid_send_init = 0;
            dir_mem[lcl][1] = EXCLUSIVE;
            dir_mem[lcl][0] = 1 << src;

            //Need to check if requesting node is same as homesite, and perform reply.

            // generate a PRI0 request to src node with updated permit_tag and data
            net_cmd_t reply_p0;
            proc_cmd_t temp = (proc_cmd_t){READ, pc.addr, 0, MODIFIED};
            copy_cache_line(temp.data, mem[lcl]);
            reply_p0.dest = src;
            reply_p0.src  = node;
            reply_p0.proc_cmd = temp;
            
            net->to_net(node, PRI0, reply_p0);
            pri2_sent_p = false;
            return(false);
          }
          else if(dir_mem[lcl][1]==SHARED){ //issue invalidation and wait
            if(!invalid_send_init){
              invalid_send_count = dir_mem[lcl][0];
              invalid_send_init = 1;
            }

            if (!pri2_sent_p){
              for(int i = 0; i<32; i++){
                // TODO: home site is remote node
                if(invalid_send_count & (1 << i)){
                  proc_cmd_t temp = (proc_cmd_t){INVALIDATE, pc.addr, 2, SHARED};  // permit tag is the home site state for PRI2         
                  net_cmd_t net_cmd;
                  net_cmd.dest = i;
                  net_cmd.src  = node;
                  net_cmd.proc_cmd = temp;
                  if(net->to_net(node, PRI2, net_cmd)){
                    invalid_send_count &= ~(1 << i);
                  }
                }
              }
              if (invalid_send_count == 0){
                pri2_sent_p = true;
              }
            }
          }
          else if(dir_mem[lcl][1]==EXCLUSIVE){ //issue invalidation and wait
            net_cmd_t invalidate_cmd;
            for(int i=0; i<32; i++){
              if(dir_mem[lcl][0] == (1<<i)){
                invalidate_cmd.dest = i;
                break;
                }
            }
            invalidate_cmd.src = node;
            proc_cmd_t temp = (proc_cmd_t){INVALIDATE, pc.addr, 2, EXCLUSIVE};
            invalidate_cmd.proc_cmd = temp;

            if(!pri2_sent_p){
              if(net->to_net(node, PRI2, invalidate_cmd)){
                pri2_sent_p = true;
              }
            }

          }
        }
        else {
          ERROR("should not have gotten a PRI3 request permit_tag other than SHARED or MODIFIED");        
        }  
      } // if (busop==READ)
      else {
        ERROR("should not have gotten a PRI3 request busop for anything other than a READ");
      }

    default:
    ERROR("Wrong bus tag for process_net_request to handle");
    return(true); // need to return something
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
// cache.c
//   by Derek Chiou
//      Oct. 8, 2023
// 

// for 382N-10

// STUDENTS: YOU ARE EXPECTED TO MODIFY THIS FILE.

#include <stdio.h>
#include "types.h"
#include "cache.h"
#include "iu.h"
#include "helpers.h"


response_t cache_t::snoop(proc_cmd_t proc_cmd) {

  response_t r;
  r.hit_p = true;  // by default, it is a hit
  r.retry_p = false;

  // case for compare LRU and generate 
  if (proc_cmd.tag == -1) {
    cache_access_response_t car;
    cache_access(proc_cmd.addr, proc_cmd.permit_tag, &car); // update the car

    //first find if there is an open spot. If open spot, return
    // int set = gen_set(proc_cmd.addr); 

    for (int a = 0; a < assoc; ++a) {
      if (tags[car.set][a].replacement == 0) { // replacement == 0 is the least recently used one
        car.way = a;
        break;
      }
    }

    if (tags[car.set][car.way].permit_tag == MODIFIED ||
          tags[car.set][car.way].permit_tag == SHARED || // in case we will later on comment out shared
        tags[car.set][car.way].permit_tag == EXCLUSIVE) { // need to writeback since replacing modified line
      proc_cmd_t wb;
      wb.busop = WRITEBACK;
      // the avaliable data including, address_tag(needed), node_id(include in address_tag), also the shifting index
      // address_tag_shift = lg_cache_line_size + lg_num_sets;
      // set_shift = lg_cache_line_size;
      wb.permit_tag = tags[car.set][car.way].permit_tag; 
      wb.tag = 1;
      wb.addr = (tags[car.set][car.way].address_tag << address_tag_shift) | (car.set << set_shift); // the remaining bits for offset are 0
      copy_cache_line(wb.data, tags[car.set][car.way].data); // dest, src.
      if (iu->from_proc_writeback(wb)) {
        ERROR("The local WB buffer should be empty from proc_cmd call");  // retry's WB has the highest priority, but snoop no need to to has the highest priority
      }
      //!!!
      tags[car.set][car.way].permit_tag = INVALID; //writeback request is put into buffer, update cache state to INVALID
      // if there is a data need to writeback, then return retry
      r.hit_p = true;
      r.retry_p = false;
      return r;
    }
    else{//INVALID
      r.hit_p = true;
      r.retry_p = false;
      return r;
    }
  }
  else if(proc_cmd.tag == 2){ //Secondary request for invalidation or confirmation

  }
  //if no open spot, try to do LRU eviction without updating the LRU bits?



}


// ***** STUDENTS: You can change writeback logic if you really need, but should be OK with the existing implementation. ***** 
void cache_t::reply(proc_cmd_t proc_cmd) {
  // fill cache.  Since processor retries until load/store completes, only need to fill cache.

  cache_access_response_t car = lru_replacement(proc_cmd.addr);  // car is the cache access response

  if(proc_cmd.tag == 0){
    NOTE_ARGS(("%d: replacing addr_tag %d into set %d, assoc %d", node, car.address_tag, car.set, car.way));
    car.permit_tag = proc_cmd.permit_tag;
    cache_fill(car, proc_cmd.data); // write data from a proc_cmd buffer to the cache
  }
  else{
    ERROR("Cache reply should only be called by bus_tag 0");
  }

}



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



// typedef struct {
//   busop_t busop;
//   address_t addr;
//   bus_tag_t tag;
//   permit_tag_t permit_tag;
//   data_t data;
// } proc_cmd_t;


// ***** FYTD ***** 
// response_t contains two boolean values, hit_p and retry_p.
response_t cache_t::snoop(proc_cmd_t proc_cmd) {
  // find the current state of the cache line
  // Depending on the current state, might need to invalidate the data (call another function to invalidate???)
  //proc_cmd_t contains a field (busop) that specifies the operation being snooped
  // depending on the operation, different actions needed

  // Procedure would be, 1: node send read, MODIFIED to local homesite or to net, other nodes keep snooping the cmd, and if they have the addr match, they will invalidate the data locally and tell home site.
  // once home site receive all invalidation, it will send the data to the node that request to modify the data
  response_t r;
  return(r);
}


// ***** STUDENTS: You can change writeback logic if you really need, but should be OK with the existing implementation. ***** 
void cache_t::reply(proc_cmd_t proc_cmd) {
  // fill cache.  Since processor retries until load/store completes, only need to fill cache.

  cache_access_response_t car = lru_replacement(proc_cmd.addr);  // car is the cache access response

  if (tags[car.set][car.way].permit_tag == MODIFIED) { // need to writeback since replacing modified line
    proc_cmd_t wb;
    wb.busop = WRITEBACK;
    // ***** FYTD: calculate the correct writeback address ***** 
    // the avaliable data including, address_tag(needed), node_id(include in address_tag), also the shifting index
    // wb.addr = 0; // need a value for now, need to calculate the correct writeback address, the pysically address of the cache line
    wb.addr = (car.address_tag << address_tag_shift) | (car.set << set_shift); // the remaining bits for offset are 0
    copy_cache_line(wb.data, tags[car.set][car.way].data); // dest, src.
    if (iu->from_proc_writeback(wb)) {
      ERROR("should not retry a from_proc_writeback since there should only be one outstanding request");
    }
  }

  NOTE_ARGS(("%d: replacing addr_tag %d into set %d, assoc %d", node, car.address_tag, car.set, car.way));
  
  car.permit_tag = proc_cmd.permit_tag;
  cache_fill(car, proc_cmd.data); // write data from a proc_cmd buffer to the cache
}


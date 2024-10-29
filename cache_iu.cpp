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


// ***** FYTD ***** 
// response_t contains two boolean values, hit_p and retry_p.
response_t cache_t::snoop(proc_cmd_t proc_cmd) {
  // The snoop need to handle the following cases:
  // 1 Snoop hit on a Read (receive a read request)
  // For example, if you have an address in E or M, and other node is reading this address, the state need to be changed to S, (WB to memory if needed)
  // 2 Snoop hit on write or RWITM (receiving invalidation request)
  // For example, if you have an address in M or E or S, and other node is writing this address, the state need to be changed to I
  // Response is to home site to indicate the operation is completed or not

  cache_access_response_t car = lru_replacement(proc_cmd.addr);  // car is the cache access response

  response_t r;
  r.hit_p = true;
  r.retry_p = false;
  // NEED: send a net reply to homesite in each case!!!
  // Consider eviction of EXCLUSIVE state should nofity the home site, to update its state ???? Confirm with Derek
  // other is reading the data, home site IU will send a READ op, with the next permit_tag is SHARED
  if (proc_cmd.busop == READ && proc_cmd.permit_tag == SHARED) {
    // sanity check, proc_cmd.tag should has the same tag as existing cache line
    if (tags[car.set][car.way].address_tag != gen_address_tag(proc_cmd.addr)) {
      ERROR("should not have gotten a read request for a different line");
    }
    if (tags[car.set][car.way].permit_tag == INVALID) {
      // message, data could be on the fly, no need to do anything
      printf("Data could be on the fly(E->I) or evicted(S->I), no need to do anything\n");
    } else if (tags[car.set][car.way].permit_tag == SHARED) {
      ERROR("should not have gotten a read request for a SHARED line to SHARED");
    } else if (tags[car.set][car.way].permit_tag == EXCLUSIVE) {
      // change the permit_tag to SHARED
      modify_permit_tag(car, SHARED);
      // create a net reply
    } else if (tags[car.set][car.way].permit_tag == MODIFIED) {
      // change the permit_tag to SHARED, writeback to memory
      proc_cmd_t wb;
      wb.busop = WRITEBACK;
      wb.addr = (tags[car.set][car.way].address_tag << address_tag_shift) | (car.set << set_shift);
      copy_cache_line(wb.data, tags[car.set][car.way].data);
      if (iu->from_proc_writeback(wb)) {  // if there is other outstanding WB request, retry
        // ERROR("should not retry a from_proc_writeback since there should only be one outstanding request");
        r.hit_p = false;
        r.retry_p = true;
        return(r);
      }
      modify_permit_tag(car, SHARED);
    }
  // other node is trying to RWITM, home site IU will send a INVALIDATE op to node who has the data, 
  // and the next permit_tag is INVALID, if the data is in M, then writeback to memory
  } else if (proc_cmd.busop == INVALIDATE && proc_cmd.permit_tag == INVALID) {
    if (tags[car.set][car.way].permit_tag == INVALID) {
      // ERROR("should not have gotten an invalidate request for an INVALID line to INVALID");
    } else if (tags[car.set][car.way].permit_tag == SHARED) {
      modify_permit_tag(car, INVALID);
    } else if (tags[car.set][car.way].permit_tag == EXCLUSIVE) {
      modify_permit_tag(car, INVALID);
    } else if (tags[car.set][car.way].permit_tag == MODIFIED) {
      proc_cmd_t wb;
      wb.busop = WRITEBACK;
      wb.addr = (tags[car.set][car.way].address_tag << address_tag_shift) | (car.set << set_shift);
      copy_cache_line(wb.data, tags[car.set][car.way].data);
      if (iu->from_proc_writeback(wb)) { 
        r.hit_p = false; 
        r.retry_p = true;
        return(r);
      }
      modify_permit_tag(car, INVALID);
    }
  } else {
    ERROR("should not reach default");
  }
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
    wb.addr = (tags[car.set][car.way].address_tag << address_tag_shift) | (car.set << set_shift); // the remaining bits for offset are 0
    copy_cache_line(wb.data, tags[car.set][car.way].data); // dest, src.
    if (iu->from_proc_writeback(wb)) {
      ERROR("should not retry a from_proc_writeback since there should only be one outstanding request");  // retry's WB has the highest priority, but snoop no need to to has the highest priority
    }
  }

  NOTE_ARGS(("%d: replacing addr_tag %d into set %d, assoc %d", node, car.address_tag, car.set, car.way));
  
  car.permit_tag = proc_cmd.permit_tag;
  cache_fill(car, proc_cmd.data); // write data from a proc_cmd buffer to the cache
}


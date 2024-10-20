// helpers.h
//   by Derek Chiou
//      Oct. 8, 2023
// 

// STUDENTS: YOU ARE NOT ALLOWED TO MODIFY THIS FILE

#include "types.h"


// this is a constant for Lab 2
const int LG_INTERLEAVE_SIZE = 8;

int gen_node(address_t addr);
int gen_local_addr(address_t addr);
int gen_local_cache_line(address_t addr);

bool within_tolerance(double, double, double);

int lg(int x);

void copy_cache_line(data_t dest, data_t src);

void terminate_on_error();

int lg(int x);

char *PRI2STRING(pri_t pri);

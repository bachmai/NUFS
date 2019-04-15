// based on cs3650 starter code

#ifndef PAGES_H
#define PAGES_H

#define NUM_PAGES 256
#define BLOCK_SIZE  4096
#define NUFS_SIZE 4096 * 256 // 1MB

#include <stdio.h>
#include <stdbool.h>

#include "inode.h"


void   pages_init(const char* path);
void   pages_free();
void*  pages_get_page(int pnum);
inode* pages_get_node(int node_id);
int    pages_find_empty();
void   print_node(inode* node);


#endif

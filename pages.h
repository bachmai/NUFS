// based on cs3650 starter code

#ifndef PAGES_H
#define PAGES_H

#define PAGE_COUNT 256
#define PAGE_SIZE  4096
#define NUFS_SIZE 4096 * 256 // 1MB

#include <stdio.h>
#include <stdbool.h>

#include "inode.h"


void   pages_init(const char* path);
inode* pages_get_node(int node_id);
int    pages_get_empty_pg();
void   print_node(inode* node);
void* pages_get_page(int pnum);
void   pages_free();
void* get_pages_bitmap();
void* get_inode_bitmap();
int alloc_page();
void free_page(int pnum);

#endif

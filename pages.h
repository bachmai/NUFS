// based on cs3650 starter code

#ifndef PAGES_H
#define PAGES_H

#define PAGE_COUNT 256
#define PAGE_SIZE 4096
#define NUFS_SIZE 4096 * 256 // 1MB

#include <stdio.h>
#include <stdbool.h>

#include "inode.h"

void pages_init(const char *path);
void pages_free();
void *pages_get_page(int pnum);
void *get_pages_bitmap();
void *get_inode_bitmap();
int alloc_page();
void free_page(int pnum);
inode *pages_get_node(int inum);
int pages_get_mt_pg();
int pages_get_mt_nd();

#endif

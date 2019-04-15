#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include "inode.h"
#include "pages.h"

typedef struct super_block {
    bool data_map[PAGE_COUNT]; // Each byte = true or false
    bool inode_map[PAGE_COUNT]; // Each byte = 1 inode, almost certainly fewer than pages
    inode* inodes;
    void* data_start;
    int num_inodes;
    int num_free_pages; // This might be removed later
    int root_inode;
} SBlock;

#endif
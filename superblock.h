#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include "inode.h"
#include "pages.h"

typedef struct super_block {
    bool db_map[PAGE_COUNT]; // boolean easier to keep track of than bitmap
    bool inodes_map[PAGE_COUNT]; 
    inode* inodes_start;
    int inodes_num;
    void* db_start;
    int db_free_num;    // number of free db
    int root_inode;     
} sp_block;

#endif
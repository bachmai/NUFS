#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include "inode.h"
#include "pages.h"

typedef struct super_block {
    bool inodes_map[PAGE_COUNT];
    bool db_map[PAGE_COUNT];
    inode* inodes_start;
    int inodes_num;
    void* db_start;
    int db_free_num;
    int root_inode;
} sp_block;

#endif
#include "superblock.h"
#include "pages.h"

// typedef struct super_block
// {
//     bool db_map[PAGE_COUNT]; // boolean easier to keep track of than bitmap
//     bool inodes_map[PAGE_COUNT];
//     inode *inodes_start;
//     int inodes_num;
//     void *db_start;
//     int db_free_num; // number of free db
//     int root_inode;
// } sp_block;

void init_superblock(sp_block *s_block)
{
    s_block->db_map[0] = 1;     // taken
    s_block->db_map[2] = 1;     // taken data_blocks starts at p2
    s_block->inodes_map[0] = 1; // taken
    s_block->inodes_map[1] = 1; // inodes table
    s_block->inodes_start = (inode *)pages_get_page(1);
    s_block->inodes_num = PAGE_SIZE / sizeof(inode);
    s_block->db_start = pages_get_page(2);
    s_block->db_free_num = PAGE_COUNT - 2;
    s_block->root_inode = 1;
}
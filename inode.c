#include <time.h>
#include <string.h>
#include <stdio.h>

#include "inode.h"
#include "pages.h"

// typedef struct inode {
//     int refs; // reference count
//     int mode; // permission & type
//     int size; // bytes
//     int ptrs[DIRECT_PTRS]; // direct pointers
//     int iptr; // single indirect pointer
//     time_t ctime; // changed time
//     time_t mtime; // modified time
// } inode;

// TODO
// inode* get_inode(int inum);
// int alloc_inode();
// void free_inode();
// int grow_inode(inode* node, int size);
// int shrink_inode(inode* node, int size);
// int inode_get_pnum(inode* node, int fpn);

void init_inode(inode *node, int mode)
{
    node->refs = 1;
    node->mode = mode;
    node->size = 0;
    node->ptrs[0] = 0;
    node->iptr = 0;
    node->ctime = time(NULL); // set time to current
    node->mtime = time(NULL);
    printf("init_inode(node, %o)\n", mode);
}

// Get the pnum of empty datablock pointed by inode
// --> no more pages ?
int get_mt_db(inode *node)
{
    for (int ii = 0; ii < DIRECT_PTRS; ++ii)
    {
        if (node->ptrs[ii] == 0) // empty
        {
            return ii;
        }
    }
    // No more available pages --
    return -1;
}

inode *
get_inode(int inum)
{
    return pages_get_node(inum);
}

// for testing
void print_inode(inode *node)
{
    if (!node)
    {
        printf("orint_node -> null\n");
    }
    else
    {
        printf("print_inode -> {refs: %d, mode: %o, size: %d, ctime: %ld, mtime: %ld}\n",
               node->refs, node->mode, node->size, node->ctime, node->mtime);
    }
}
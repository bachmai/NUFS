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
//     time_t ctime; // created time
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
    for (int ii = 0; ii < DIRECT_PTRS; ii++)
    {
        node->ptrs[ii] = 0;
    }
    node->iptr = 0;
    node->size = 0;
    node->ctime = time(NULL); // set time to current
    node->mtime = time(NULL);
    printf("init_inode(node, %o)\n", mode);
}

void inode_set_ptrs(inode *node, int pnum, int data_size)
{

    for (int ii = 0; ii < DIRECT_PTRS; ii++)
    {
        if (node->ptrs[ii] == 0)
        {
            node->ptrs[ii] = pnum;
            break;
        }
    }
    node->size += data_size;
    printf("inode_set_ptrs(node, %d, %d)n", pnum, data_size);
}

int inode_write_helper(inode *node, const char *buf, size_t bytes, off_t offset)
{
    int rv = -1;

    // TODO
    if (bytes > PAGE_SIZE || node->ptrs[0] == 0)
    {
        printf("inode_write_helper(%s, %ld, %ld) -> %d\n", buf, bytes, offset, rv);
        return rv;
    }

    memcpy(pages_get_page(node->ptrs[0]), buf, bytes);
    node->mtime = time(NULL);
    node->size = bytes; // offset?

    rv = bytes;
    printf("inode_write_helper(%s, %ld, %ld) -> %d\n", buf, bytes, offset, rv);
    return rv;
}

void print_inode(inode *node)
{
    if (!node)
    {
        printf("given node is null\n");
    }
    else
    {
        printf("print_inode -> {refs: %d, mode: %o, size: %d, ctime: %ld, mtime: %ld}\n",
               node->refs, node->mode, node->size, node->ctime, node->mtime);
    }
}

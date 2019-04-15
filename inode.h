// based on cs3650 starter code

#ifndef INODE_H
#define INODE_H

#include <fcntl.h>
#include <sys/stat.h>

// #include "pages.h"

#define NUM_BLOCKS 8

typedef struct inode {
    // int refs; // reference count
    // int mode; // permission & type
    // int size; // bytes
    // int ptrs[2]; // direct pointers
    // int iptr; // single indirect pointer
    int refs; // references to this inode
    int mode; // permissions and file type
    time_t ctime; // creation time
    time_t mtime; // modification time
    int data[NUM_BLOCKS]; // each entry is a pnum
    int indirect_pum;
    int size; // bytes for file
} inode;

// void print_inode(inode* node);
// inode* get_inode(int inum);
// int alloc_inode();
// void free_inode();
// int grow_inode(inode* node, int size);
// int shrink_inode(inode* node, int size);
// int inode_get_pnum(inode* node, int fpn);

inode* make_inode();
void inode_init(inode* node, int mode);
void inode_set_data(inode* node, int pnum, int data_size);
int inode_write(inode* node, const char* buf, size_t bytes, off_t offset);
void print_inode(inode* node);

#endif

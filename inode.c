#include <time.h>
#include <string.h>
#include <stdio.h>

#include "inode.h"
#include "pages.h"

// typedef struct inode {
//     int refs; // reference count
//     int mode; // permission & type
//     int size; // bytes
//     int ptrs[2]; // direct pointers
//     int iptr; // single indirect pointer
// } inode;

// TODO
// void print_inode(inode* node);
// inode* get_inode(int inum);
// int alloc_inode();
// void free_inode();
// int grow_inode(inode* node, int size);
// int shrink_inode(inode* node, int size);
// int inode_get_pnum(inode* node, int fpn);

// This only gets called when a node is first created
void inode_init(inode* node, int mode)
{
    printf("inode_init(node, %o)\n", mode);

    // update metadata
    node->refs = 1;
    node->mode = mode;
    node->ctime = time(NULL);
    node->mtime = time(NULL);
    node->indirect_pum = 0;
    node->size = 0; // nothing written yet

    for (int ii = 0; ii < NUM_BLOCKS; ii++) {
        node->data[ii] = 0;
    }
}

// There might be an issue if we have all blocks filled up TODO
void inode_set_data(inode* node, int pnum, int data_size){ 
    printf("inode_set_data(node, %d, %d\n", pnum, data_size);
    for (int ii = 0; ii < NUM_BLOCKS; ii++ ) {
        if (node->data[ii] == 0) {
            node->data[ii] = pnum;
            break;
        }
    }

    node->size += data_size;
    printf("inode_set_data, now node size is %d\n", node->size);
}

int inode_write(inode* node, const char* buf, size_t bytes, off_t offset) {
    printf("inode_write(%s, %ld, %ld)\n", buf, bytes, offset);
    // TODO: Worry about big stuff
    if (bytes > BLOCK_SIZE) {
        return -1;
    }

    printf("node->data[0] = %d\n", node->data[0]);
    if (node->data[0] == 0) {
        return -1;
    }

    memcpy(pages_get_page(node->data[0]), buf, bytes);

    node->size = bytes; // TODO: prolly add offset
    node->mtime = time(NULL);

    return bytes;
}

void print_inode(inode* node)
{
    if (node) {
        printf("inode{ refs: %d, mode: %o, size: %d}\n", node->refs, node->mode, node->size);
    } else {
        printf("given node is null\n");
    }
}

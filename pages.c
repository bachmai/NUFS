// based on cs3650 starter code

#define _GNU_SOURCE
#include <string.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "pages.h"
#include "util.h"
#include "bitmap.h"

const int PAGE_COUNT = 256;

static int   pages_fd   = -1;
static void* pages_base =  0;

static SBlock* sb;

void
pages_init(const char* path)
{
    printf("pages_init(%s)\n", path);
    pages_fd = open(path, O_CREAT | O_RDWR, 0644);
    assert(pages_fd != -1);

    int rv = ftruncate(pages_fd, NUFS_SIZE);
    assert(rv == 0);

    pages_base = mmap(0, NUFS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, pages_fd, 0);
    assert(pages_base != MAP_FAILED);

    sb = (SBlock*)pages_base;

    //Initialize Data and INode maps
    for (int i = 0; i < NUM_PAGES; i++) {
        sb->data_map[i] = false;
        sb->inode_map[i] = false;
    }

    // Initialize Other fields
    sb->inodes = (inode*)pages_get_page(1); // Inodes are in block 1
    sb->data_start  = pages_get_page(2);    // Data starts at block 2
    sb->num_inodes = BLOCK_SIZE / sizeof(inode);
    sb->num_free_pages = NUM_PAGES - 2; // Minus root and inode section

    // Node 0 is just going to be NULL I guess
    sb->inode_map[0] = true;
    sb->data_map[0] = true;
    printf("end pages_init(%s)\n", path);
}

inode*
pages_get_node(int node_id)
{
    printf("pages_get_node(%d)\n", node_id);
    if (node_id < sb->num_inodes) {
        inode* idx = &(sb->inodes[node_id]);
        return idx;
    }
    return NULL; // invalid node
}

int
pages_find_empty()
{
    for (int ii = 2; ii < PAGE_COUNT; ++ii) {
        if (sb->data_map[ii] == false) { // if page is empty
            printf("pages_find_empty() -> %d\n", ii);
            return ii;
        }
    }
    printf("pages_find_empty() -> %d\n", -1);
    return -1;
}

void
print_node(inode* node)
{
    if (node) {
        printf("node{refs: %d, mode: %04o, size: %d}\n",
               node->refs, node->mode, node->size);
    }
    else {
        printf("node{null}\n");
    }
}

void
pages_free()
{
    int rv = munmap(pages_base, NUFS_SIZE);
    assert(rv == 0);
}

void*
pages_get_page(int pnum)
{
    return pages_base + 4096 * pnum;
}

void*
get_pages_bitmap()
{
    return pages_get_page(0);
}

void*
get_inode_bitmap()
{
    uint8_t* page = pages_get_page(0);
    return (void*)(page + 32);
}

int
alloc_page()
{
    void* pbm = get_pages_bitmap();

    for (int ii = 1; ii < PAGE_COUNT; ++ii) {
        if (!bitmap_get(pbm, ii)) {
            bitmap_put(pbm, ii, 1);
            printf("+ alloc_page() -> %d\n", ii);
            return ii;
        }
    }

    return -1;
}

void
free_page(int pnum)
{
    printf("+ free_page(%d)\n", pnum);
    void* pbm = get_pages_bitmap();
    bitmap_put(pbm, pnum, 0);
}


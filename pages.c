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
#include "superblock.h"

static int pages_fd = -1;
static void *pages_base = 0;

static sp_block *s_block; // Our superblock

// Superblock : pg 0
// inodes : pg 1
// pages : pg 2

void pages_init(const char *path)
{

    pages_fd = open(path, O_CREAT | O_RDWR, 0644);
    assert(pages_fd != -1);

    int rv = ftruncate(pages_fd, NUFS_SIZE);
    assert(rv == 0);

    pages_base = mmap(0, NUFS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, pages_fd, 0);
    assert(pages_base != MAP_FAILED);

    s_block = (sp_block *)pages_base;

    // init the superblock
    s_block->inodes_start = (inode *)pages_get_page(1);
    s_block->inodes_num = PAGE_SIZE / sizeof(inode);
    s_block->db_start = pages_get_page(2);
    s_block->db_free_num = PAGE_COUNT - 2;
    s_block->inodes_map[0] = 1; // taken
    s_block->db_map[0] = 1;     // taken

    printf("pages_init(%s) -> done\n", path);
}

// Find and returns the inum-th inode from superrblock
inode *
pages_get_node(int inum)
{
    // printf("pages_get_node(%d)\n", inum);
    inode *rv = 0;  // empty
    if (inum < s_block->inodes_num)
    {
        inode *node = &(s_block->inodes_start[inum]);
        printf("pages_get_node(%d)\n -> success\n", inum);
        rv = node;
    }
    return rv;
}

int pages_get_empty_pg()
{
    int rv = -1;
    // Traverse through db
    for (int ii = 2; ii < PAGE_COUNT; ++ii)
    {
        if (s_block->db_map[ii] == 0)
        {
            rv = ii;
            break;
        }
    }
    printf("pages_get_empty_pg() -> %d\n", rv);
    return rv;
}

void pages_free()
{
    int rv = munmap(pages_base, NUFS_SIZE);
    assert(rv == 0);
}

void *
pages_get_page(int pnum)
{
    return pages_base + 4096 * pnum;
}

void *
get_pages_bitmap()
{
    return pages_get_page(0);
}

void *
get_inode_bitmap()
{
    uint8_t *page = pages_get_page(0);
    return (void *)(page + 32);
}

int alloc_page()
{
    void *pbm = get_pages_bitmap();

    for (int ii = 1; ii < PAGE_COUNT; ++ii)
    {
        if (!bitmap_get(pbm, ii))
        {
            bitmap_put(pbm, ii, 1);
            printf("+ alloc_page() -> %d\n", ii);
            return ii;
        }
    }

    return -1;
}

void free_page(int pnum)
{
    printf("+ free_page(%d)\n", pnum);
    void *pbm = get_pages_bitmap();
    bitmap_put(pbm, pnum, 0);
}

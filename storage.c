#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <libgen.h>
#include <errno.h>
#include <stdint.h>

#include "directory.h"
#include "pages.h"
#include "storage.h"
#include "util.h"
#include "inode.h"
#include "superblock.h"

static sp_block *s_block = NULL;

void storage_init(const char *path)
{
    pages_init(path); // initialize pages for storage

    // superblock goes into 0th page
    s_block = (sp_block *)pages_get_page(0);
    s_block->root_inode = 1;        // root inode stored at 1st page
    s_block->inodes_map[1] = 1;

    // init root directory
    directory_init();
    s_block->db_map[2] = 1; // data_blocks start at page 2
    printf("storage_init(%s)\n", path);
}

// helper that returns the inode related to given path
static inode *
get_node_from_path(const char *path)
{
    int inum = tree_lookup(path);       // find the node index in directory
    inode* rv = &(s_block->inodes_start[inum]);
    if (inum <= 0)
    {
        rv = NULL;
    }
    return rv;
}

int storage_stat(const char *path, struct stat *st)
{ 
    inode *node = get_node_from_path(path);
    print_node(node);
    if (!node)
    {
        return -ENOENT;
    }
    st->st_size = node->size;
    st->st_mode = node->mode;
    st->st_nlink = node->refs;
    st->st_mtime = node->mtime;
    printf("storage_stat(%s)\n", path);
    return 0;
}

// returns the first empty inode index if any
int get_empty_node()
{
    int rv = -1;
    for (int ii = 2; ii < PAGE_COUNT; ii++)
    {
        if (!s_block->inodes_map[ii]) 
        {
            rv = ii;
            break;
        }
    }
    return rv;
}

int storage_read(const char *path, char *buf, size_t size, off_t offset)
{
    int rv = -1;
    inode *node = get_node_from_path(path); 
    
    if (!node) 
    {
        return rv;
    }
    void *page = pages_get_page(0);

    rv = clamp(node->size - offset, 0, size);       // bytes read max possible
    memcpy(page + offset, buf, rv);
    printf("storage_read(%s) -> %d\n", path, rv);
    return rv;
}

int storage_write(const char *path, const char *buf, size_t size, off_t offset)
{
    int rv = -1;
    inode *node = get_node_from_path(path); 
    if (!node) 
    {
        return rv;
    }

    if (size > PAGE_SIZE || node->ptrs[0] == 0)
    {
        printf("storage_write(%s, %s, %ld, %ld) -> %d\n", path, buf, size, offset, rv);
        return rv;
    }
    void* page_to_write = pages_get_page(node->ptrs[0]);
    memcpy(page_to_write, buf, size);
    node->mtime = time(NULL);
    node->size = size;

    rv = size;
    printf("storage_write(%s, %s, %ld, %ld) -> %d\n", path, buf, size, offset, rv);
    return rv;
}

int storage_truncate(const char *path, off_t size)
{
    int rv = -1;
    if (size > PAGE_SIZE)
    {
        return rv;
    }

    inode *node = get_node_from_path(path);
    if (!node) 
    {
        return rv;
    }
    node->size = size;
    
    rv = 0; // success
    printf("storage_truncate(%s, %ld)\n", path, size);
    return rv;
}

int storage_mknod(const char *path, mode_t mode)
{
    // work around - got this from stackOverflow
    char *path1 = alloca(strlen(path));
    char *path2 = alloca(strlen(path));
    strcpy(path1, path);
    strcpy(path2, path);
    char *par_dir = dirname(path1);    // parent directory   
    char *cur_dir = basename(path2);   // curent directory

    directory dir = get_dir_path(par_dir);
    if (!dir.node) // no such directory
    {
        return -ENOENT;
    }

    if (directory_lookup(dir, cur_dir) != -ENOENT)  // file already exist
    {
        return -EEXIST;
    }

    int pnum = pages_get_empty_pg();
    assert(pnum > 2);

    int inum = get_empty_node();
    assert(inum > 1);

    inode *node = pages_get_node(inum);
    init_inode(node, mode);
    inode_set_ptrs(node, pnum, 0);

    // update superblock 
    s_block->db_map[pnum] = 1;
    s_block->inodes_map[inum] = 1;
    s_block->inodes_start[inum] = *(node);

    printf("storage_mknod(%s, %o)\n", path, mode);
    return directory_put(dir, cur_dir, inum);
}

int storage_unlink(const char *path)
{
    char *path1 = alloca(strlen(path));
    char *path2 = alloca(strlen(path));
    strcpy(path1, path);
    strcpy(path2, path);
    char *par_dir = dirname(path1);
    char *cur_dir = basename(path2);

    int p_index = tree_lookup(par_dir);
    if (p_index <= 0)
    {
        return -1;
    }

    directory p_dir = get_dir_inum(p_index);
    int inum = directory_delete(p_dir, (const char *)cur_dir);
    if (inum <= 0)
    {
        return -1;
    }

    inode *node = &(s_block->inodes_start[inum]);

    // FIXME. if refs == 1 clear all links
    if (node->refs == 1)
    {
        for (int ii = 0; ii < DIRECT_PTRS; ++ii)
        {
            int pnum = node->ptrs[ii];
            if (pnum > 2)
            {
                s_block->db_map[pnum] = 0;
                node->ptrs[ii] = 0;
            }
        }
        node->size = 0;
        s_block->inodes_map[inum] = 0;
    }
    printf("storage_unlink(%s)\n", path);
    return 0;
}

int storage_link(const char *from, const char *to)
{

    int inum = tree_lookup(from);
    if (inum <= 0)
    {
        return -1;
    }

    inode *node = &(s_block->inodes_start[inum]);

    char *path1 = alloca(strlen(to));
    char *path2 = alloca(strlen(to));
    strcpy(path1, to);
    strcpy(path2, to);
    char *par_dir = dirname(path1);
    char *cur_dir = basename(path2);

    directory up_one_dir = get_dir_path(par_dir);
    if (up_one_dir.pnum <= 0)
    {
        return up_one_dir.pnum;
    }

    int linked = directory_put(up_one_dir, (const char *)cur_dir, inum);
    if (linked == 0)
    {
        node->refs += 1;
    }

    printf("storage_link(%s, %s)\n", from, to);
    return linked;
}

int storage_rename(const char *from, const char *to)
{
    if (storage_link(from, to) < 0)
    {
        return -1;
    }

    printf("storage_rename(%s, %s)\n", from, to);
    return storage_unlink(from);
}

int storage_set_time(const char *path, const struct timespec ts[2])
{
    int inum = tree_lookup(path);
    if (inum <= 0)
    {
        return -1;
    }

    inode *node = &(s_block->inodes_start[inum]);
    node->ctime = ts[0].tv_sec;
    node->mtime = ts[1].tv_sec;

    printf("storage_set_time(%s)\n", path);
    return 0;
}

const char *
storage_data(const char *path)
{
    inode *node = get_node_from_path(path);
    if (!node)
    {
        return 0;
    }

    for (int ii = 0; ii < DIRECT_PTRS; ++ii)
    {
        int pnum = node->ptrs[ii];
        if (pnum == 0)
        {
            return 0;
        }

        // TODO
        const char *data = (const char *)pages_get_page(pnum);
        printf("storage_data(%s) -> %s\n", path, data);
        return data;
    }
}

int storage_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                    off_t offset)
{
    directory dir = get_dir_path(path);
    struct stat st;
    char path_temp[256];

    for (int ii = 0; ii < DIR_LIMIT; ++ii)
    {
        char *name = dir.dirents[ii].name;
        if (name[0] != 0)
        {
            strcpy(path_temp, path);
            strcat(path_temp, name);

            storage_stat(path_temp, &st);
            filler(buf, name, &st, 0);
        }
    }
    printf("storage_readdir(%s)\n", path);

    return 0;
}

int storage_mkdir(const char *path, mode_t mode)
{
    char *path1 = alloca(strlen(path));
    char *path2 = alloca(strlen(path));
    strcpy(path1, path);
    strcpy(path2, path);
    char *par_dir = dirname(path1);
    char *cur_dir = basename(path2);

    directory dir = get_dir_path(path1);
    assert(dir.pnum > 0);

    int pnum = pages_get_empty_pg();
    assert(pnum > 0);

    int inum = get_empty_node();
    assert(inum > 0);

    // init pointers
    inode *node = &(s_block->inodes_start[inum]);
    mode_t dir_mode = (mode |= 040000);
    init_inode(node, dir_mode);
    inode_set_ptrs(node, pnum, PAGE_SIZE);

    // update superblock
    s_block->inodes_map[inum] = 1;
    s_block->db_map[pnum] = 1;

    printf("storage_mkdir(%s, %o)\n", path, mode);
    return directory_put(dir, cur_dir, inum);
}

int storage_rmdir(const char *path)
{
    // cannot delete nonempty dir
    int inum = rm_dir(path);
    if (inum <= 0)
    {
        return -1;
    }

    inode *node = &(s_block->inodes_start[inum]);
    if (node->refs == 1)
    {
        for (int ii = 0; ii < DIRECT_PTRS; ++ii)
        {
            int pnum = node->ptrs[ii];
            if (pnum > 2)
            {
                s_block->db_map[pnum] = 0;
                node->ptrs[ii] = 0;
            }
        }
        node->size = 0;
        s_block->inodes_map[inum] = 0;
    }
    printf("storage_rmdir(%s)\n", path);
    return 0;
}

int storage_chmod(const char *path, mode_t mode)
{
    int inum = tree_lookup(path);
    if (inum <= 0)
    {
        return -1;
    }

    inode *node = &(s_block->inodes_start[inum]);
    node->mode = mode;

    printf("storage_chmod(%s, %o)\n", path, mode);
    return 0;
}

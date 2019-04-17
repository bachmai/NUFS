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
    pages_init(path);

    s_block = (sp_block *)pages_get_page(0);
    s_block->root_inode = 1;
    s_block->inodes_map[1] = true;

    // init root directory
    directory_init();
    s_block->db_map[2] = true; //
}

static inode *
get_node_from_path(const char *path)
{
    int inum = tree_lookup(path);

    if (inum <= 0)
    {
        return NULL;
    }

    printf("get_node_from_path(%s) -> %d\n", path, inum);

    return &(s_block->inodes_start[inum]);
}

int storage_stat(const char *path, struct stat *st)
{
    printf("storage_stat(%s)\n", path);
    inode *node = get_node_from_path(path);
    print_node(node);
    if (!node)
    {
        return -ENOENT;
    }

    st->st_mode = node->mode;
    st->st_nlink = node->refs;
    st->st_size = node->size;
    st->st_mtime = node->mtime;

    return 0;
}

int get_empty_node()
{
    if (!s_block)
    {
        return -1;
    }

    for (int ii = 2; ii < PAGE_COUNT; ii++)
    {
        if (!s_block->inodes_map[ii])
        {
            return ii;
        }
    }
    return -1;
}

int storage_read(const char *path, char *buf, size_t size, off_t offset)
{

    if (offset + size > PAGE_SIZE)
    {
        return -1;
    }

    int inum = tree_lookup(path);
    if (inum <= 0)
    {
        return -1;
    }

    inode *node = &(s_block->inodes_start[inum]);

    int pnum = 0;
    void *page = pages_get_page(pnum);

    int count = clamp(node->size - offset, 0, size);
    memcpy(page + offset, buf, count);
    printf("storage_read(%s) -> %d\n", path, count);
    return count;
}

int storage_write(const char *path, const char *buf, size_t size, off_t offset)
{

    if (offset + size > PAGE_SIZE)
    {
        return -1;
    }

    int inum = tree_lookup(path);
    if (inum <= 0)
    {
        return -1;
    }

    inode *node = &(s_block->inodes_start[inum]);
    print_node(node);

    printf("storage_write(%s)\n", path);
    return inode_write_helper(node, buf, size, offset);
}

int storage_truncate(const char *path, off_t size)
{

    if (size > 4096)
    {
        return -1;
    }

    int inum = tree_lookup(path);
    if (inum <= 0)
    {
        return -1;
    }

    inode *node = &(s_block->inodes_start[inum]);
    node->size = size;

    printf("storage_truncate(%s)\n", path);
    return 0;
}

int storage_mknod(const char *path, mode_t mode)
{
    printf("storage_mknod(%s, %o)\n", path, mode);
    char *path1 = alloca(strlen(path));
    char *path2 = alloca(strlen(path));
    strcpy(path1, path);
    strcpy(path2, path);
    char *dir_name = dirname(path1);
    char *base_name = basename(path2);

    printf("storage_mknod: dir_name(%s), name(%s)\n", dir_name, base_name);
    directory dir = get_dir_path(dir_name);
    if (dir.node == 0)
    {
        return -ENOENT;
    }
    printf("storage_mknod: done with get_dir_path\n");
    if (directory_lookup(dir, base_name) != -ENOENT)
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

    // add file to parent directory
    return directory_put(dir, base_name, inum);
}

int storage_unlink(const char *path)
{

    char *path1 = alloca(strlen(path));
    char *path2 = alloca(strlen(path));
    strcpy(path1, path);
    strcpy(path2, path);
    char *dir_name = dirname(path1);
    char *base_name = basename(path2);

    int p_index = tree_lookup(dir_name);
    if (p_index <= 0)
    {
        return -1;
    }

    directory p_dir = get_dir_inum(p_index);
    int inum = directory_delete(p_dir, (const char *)base_name);
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
    char *dir_name = dirname(path1);
    char *base_name = basename(path2);

    directory up_one_dir = get_dir_path(dir_name);
    if (up_one_dir.pnum <= 0)
    {
        return up_one_dir.pnum;
    }

    int linked = directory_put(up_one_dir, (const char *)base_name, inum);
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
    char *dir_name = dirname(path1);
    char *base_name = basename(path2);

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
    s_block->inodes_map[inum] = true;
    s_block->db_map[pnum] = true;

    printf("storage_mkdir(%s, %o)\n", path, mode);
    return directory_put(dir, base_name, inum);
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

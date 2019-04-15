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

    s_block = (sp_block*)pages_get_page(0);
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

    memset(st, 0, sizeof(struct stat));
    st->st_mode = node->mode;
    st->st_nlink = node->refs;
    st->st_size = node->size;

    struct timespec mtime = {node->mtime, (long)node->mtime};
    st->st_mtim = mtime;

    return 0;
}

int get_empty_node()
{
    if (s_block == NULL)
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

const char *
storage_data(const char *path)
{
    printf("storage_data(%s)\n", path);
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

    int node_idx = get_empty_node();
    assert(node_idx > 1);

    inode *node = pages_get_node(node_idx);
    init_inode(node, mode);
    inode_set_ptrs(node, pnum, 0); // no data stored yet

    // Update the metadata
    s_block->db_map[pnum] = true;
    s_block->inodes_map[node_idx] = true;
    s_block->inodes_start[node_idx] = *(node);

    // add file to parent directory
    return directory_put(dir, base_name, node_idx);
}

int storage_mkdir(const char *path, mode_t mode)
{
    printf("storage_mkdir(%s, %o)\n", path, mode);
    char *tmp1 = alloca(strlen(path));
    char *tmp2 = alloca(strlen(path));
    strcpy(tmp1, path);
    strcpy(tmp2, path);

    char *dname = dirname(tmp1);
    char *name = basename(tmp2);

    directory dir = get_dir_path(tmp1);
    assert(dir.pnum > 0);

    int pnum = pages_get_empty_pg();
    assert(pnum > 0);

    // create inode
    int inode_num = get_empty_node();
    assert(inode_num > 0);

    // initialize data
    inode *node = &(s_block->inodes_start[inode_num]);
    mode_t dir_mode = (mode |= 040000);
    printf("dir_mode: %o\n", dir_mode);
    init_inode(node, dir_mode);
    inode_set_ptrs(node, pnum, PAGE_SIZE);

    // update the metadata
    s_block->inodes_map[inode_num] = true; // this is used
    s_block->db_map[pnum] = true;

    // add directory to parent
    return directory_put(dir, name, inode_num);
}

int storage_link(const char *from, const char *to)
{
    printf("storage_link(%s, %s)\n", from, to);

    int inum = tree_lookup(from);
    if (inum <= 0)
    {
        return inum;
    }

    inode *node = &(s_block->inodes_start[inum]);

    char *tmp1 = alloca(strlen(to));
    char *tmp2 = alloca(strlen(to));
    strcpy(tmp1, to);
    strcpy(tmp2, to);

    char *dname = dirname(tmp1);
    char *name = basename(tmp2);

    directory to_parent = get_dir_path(dname);
    if (to_parent.pnum <= 0)
    {
        return to_parent.pnum;
    }

    int linked = directory_put(to_parent, (const char *)name, inum);
    if (linked == 0)
    {
        node->refs += 1;
    }

    return linked;
}

int storage_unlink(const char *path)
{
    printf("storage_unlink(%s)\n", path);

    char *tmp1 = alloca(strlen(path));
    char *tmp2 = alloca(strlen(path));
    strcpy(tmp1, path);
    strcpy(tmp2, path);

    char *dname = dirname(tmp1);
    char *name = basename(tmp2);

    int parent_inum = tree_lookup(dname);
    if (parent_inum <= 0)
    {
        return -1;
    }

    directory parent_dir = get_dir_inum(parent_inum);
    int inum = directory_delete(parent_dir, (const char *)name);
    if (inum <= 0)
    {
        return -1;
    }

    inode *node = &(s_block->inodes_start[inum]);

    printf("storage_unlink: s_block->inodes_start[inum].refs = %d\n", node->refs);

    // if there are no more references, clear all the links
    if (--(node->refs) == 0)
    {
        for (int ii = 0; ii < DIRECT_PTRS; ++ii)
        {
            int pnum = node->ptrs[ii];
            if (pnum > 2)
            { // pages 2 and below are vital
                s_block->db_map[pnum] = false;
                node->ptrs[ii] = 0;
            }
        }
        node->size = 0;
        s_block->inodes_map[inum] = false;
    }
    return 0;
}

int storage_rmdir(const char *path)
{
    // Return an error if the user tries to remove a non-empty directory
    // The error 'ENOTEMPTY' is probably the right one
    int inum = rm_dir(path);
    if (inum < 0)
        return inum;
    if (inum == 0)
        return -1;

    inode *node = &(s_block->inodes_start[inum]);
    if (--(node->refs) == 0)
    {
        for (int ii = 0; ii < DIRECT_PTRS; ++ii)
        {
            int pnum = node->ptrs[ii];
            if (pnum > 2)
            { // pages 2 and below are vital
                s_block->db_map[pnum] = false;
                node->ptrs[ii] = 0;
            }
        }
        node->size = 0;
        s_block->inodes_map[inum] = false;
    }
    return 0;
}

int storage_rename(const char *from, const char *to)
{
    if (storage_link(from, to) < 0)
    {
        return -1;
    }

    return storage_unlink(from);
}

int storage_chmod(const char *path, mode_t mode)
{
    printf("storage_chmod(%s, %o)\n", path, mode);
    int inum = tree_lookup(path);
    if (inum <= 0)
    {
        return inum;
    }

    inode *node = &(s_block->inodes_start[inum]);
    node->mode = mode;

    return 0;
}

int storage_truncate(const char *path, off_t size)
{
    printf("storage_read(%s)\n", path);
    if (size > 4096)
    {
        return -1;
    }

    int inum = tree_lookup(path);
    if (inum <= 0)
    {
        return inum;
    }

    inode *node = &(s_block->inodes_start[inum]);
    node->size = size;

    // TODO: zero out the bytes when expanding a file if you have debris
    // left from shrinking

    return 0;
}

int storage_read(const char *path, char *buf, size_t size, off_t offset)
{
    printf("storage_read(%s)\n", path);
    if (offset + size > PAGE_SIZE)
    {
        // TODO: implement large files
        return -1;
    }

    int inum = tree_lookup(path);
    if (inum <= 0)
    {
        return inum;
    }

    inode *node = &(s_block->inodes_start[inum]);

    // TODO loop and get info from node
    int pnum = 0; // FIXME this is a hack to get 'make' to work
    void *page = pages_get_page(pnum);

    int count = clamp(node->size - offset, 0, size);
    memcpy(page + offset, buf, count);
    return count;
}

int storage_write(const char *path, const char *buf, size_t size, off_t offset)
{
    printf("storage_write(%s)\n", path);
    if (offset + size > PAGE_SIZE)
    {
        // TODO: implement large files
        return -1;
    }

    int inum = tree_lookup(path);
    if (inum <= 0)
    {
        return inum;
    }

    inode *node = &(s_block->inodes_start[inum]);
    print_node(node);

    return inode_write_helper(node, buf, size, offset);
}

int storage_set_time(const char *path, const struct timespec ts[2])
{
    int inum = tree_lookup(path);
    if (inum <= 0)
    {
        return inum;
    }

    inode *node = &(s_block->inodes_start[inum]);
    node->ctime = ts[0].tv_sec;
    node->mtime = ts[1].tv_sec;

    return 0;
}

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

// #define NUFS_SIZE 1024 * 1024 // 1 MB

static SBlock* sb = NULL;

int 
get_free_inode() {
    if (sb == NULL) {
        return -1;
    }

    for (int ii = 2; ii < NUM_PAGES; ii++) {
        if (!sb->inode_map[ii]) {
            return ii;
        }
    }
    return -1;
}

void
storage_init(const char* path)
{
    // Initialize the pages and store the file system data in the given path
    pages_init(path);

    // Allocate root directory at 1
    sb = (SBlock*)pages_get_page(0);
    
    
    sb->root_inode = 1;
    sb->inode_map[1] = true; 
    
    // Initializes the root directory in inode 1, page 2
    directory_init();
    sb->data_map[2] = true;
}

static inode*
lookup_inode(const char* path) 
{
    printf("lookup_inode(%s)\n", path);
    int inum = tree_lookup_inum(path);
    printf("lookup_inode(%s) -> %d\n", path, inum);

    if (inum <= 0) {
        return NULL;
    }

    return &(sb->inodes[inum]);
}

int
check_access(const char* path, int mask)
{
    printf("check_access(%s, %d)", path, mask);
    inode* node = lookup_inode(path);
    printf("check_access node %o\n", node->mode);
}

int
get_stat(const char* path, struct stat* st)
{
    printf("get_stat(%s)\n", path);
    inode* node = lookup_inode(path);
    print_node(node);
    if (!node) {
        return -1;
    }

    memset(st, 0, sizeof(struct stat));
    st->st_mode = node->mode;
    st->st_nlink = node->refs;
    st->st_uid  = getuid();
    st->st_size = node->size;

    struct timespec mtime = { node->mtime, (long) node->mtime};
    st->st_mtim = mtime;

    return 0;
}

const char*
get_data(const char* path)
{
    printf("get_data(%s)\n", path);
    inode* node = lookup_inode(path);
    if (!node) {
        return 0;
    }

    for (int ii = 0; ii < NUM_BLOCKS; ++ii) {
        int pnum = node->data[ii];
        if (pnum == 0) {
            return 0;
        }

        // TODO get the data from all the pages, not just the
        // first one
        const char* data = (const char*) pages_get_page(pnum);
        printf("get_data(%s) -> %s\n", path, data);
        return data;
    }
}

int 
storage_readdir(const char* path, void* buf, fuse_fill_dir_t filler, 
        off_t offset) 
{
    printf("storage_readdir(%s)\n", path);
    directory dir = directory_from_path(path);
    struct stat st;
    char full_path[256];

    for (int ii = 0; ii < DIR_SIZE; ++ii) {
        char* name = dir.ents[ii].name;
        if (name[0] != 0) {
            strcpy(full_path, path);
            strcat(full_path, name);

            get_stat(full_path, &st);
            filler(buf, name, &st, 0); 
        }
    }

    return 0;
}

int
storage_mknod(const char* path, mode_t mode)
{
    printf("storage_mknod(%s, %o)\n", path, mode);
    char* tmp1 = alloca(strlen(path));
    char* tmp2 = alloca(strlen(path));
    strcpy(tmp1, path);
    strcpy(tmp2, path);

    char* dname = dirname(tmp1);
    char* name = basename(tmp2);

    printf("storage_mknod: dname(%s), name(%s)\n", dname, name);
    directory dir = directory_from_path(dname);
    if (dir.node == 0) {
        return -ENOENT;
    }
    printf("storage_mknod: done with directory_from_path\n");
    if (directory_lookup_inum(dir, name) != -ENOENT) {
        return -EEXIST;
    }

    int pnum = pages_find_empty();
    assert(pnum > 2); // root is at page 2

    int node_idx = get_free_inode();
    assert(node_idx > 1); // root is at inode 1

    inode* node = pages_get_node(node_idx);
    inode_init(node, mode);
    inode_set_data(node, pnum, 0); // no data stored yet

    // Update the metadata
    sb->data_map[pnum] = true;
    sb->inode_map[node_idx] = true;
    sb->inodes[node_idx] = *(node);

    // add file to parent directory
    return directory_put_ent(dir, name, node_idx);
}

int 
storage_mkdir(const char* path, mode_t mode)
{
    printf("storage_mkdir(%s, %o)\n", path, mode);
    char* tmp1 = alloca(strlen(path));
    char* tmp2 = alloca(strlen(path));
    strcpy(tmp1, path);
    strcpy(tmp2, path);

    char* dname = dirname(tmp1);
    char* name = basename(tmp2);

    directory dir = directory_from_path(tmp1);
    assert(dir.pnum > 0);

    int pnum = pages_find_empty();
    assert(pnum > 0);

    // create inode
    int inode_num = get_free_inode();
    assert(inode_num > 0);

    // initialize data
    inode* node = &(sb->inodes[inode_num]);
    mode_t dir_mode = (mode |= 040000);
    printf("dir_mode: %o\n", dir_mode);
    inode_init(node, dir_mode);
    inode_set_data(node, pnum, BLOCK_SIZE);

    // update the metadata
    sb->inode_map[inode_num] = true;  // this is used
    sb->data_map[pnum] = true;

    // add directory to parent
    return directory_put_ent(dir, name, inode_num);
}

int 
storage_link(const char* from, const char* to)
{
    printf("storage_link(%s, %s)\n", from, to);

    int inum = tree_lookup_inum(from);
    if (inum <= 0) {
        return inum;
    }

    inode* node = &(sb->inodes[inum]);

    char* tmp1 = alloca(strlen(to));
    char* tmp2 = alloca(strlen(to));
    strcpy(tmp1, to);
    strcpy(tmp2, to);

    char* dname = dirname(tmp1);
    char* name = basename(tmp2);

    directory to_parent = directory_from_path(dname);
    if (to_parent.pnum <= 0) {
        return to_parent.pnum;
    }

    int linked = directory_put_ent(to_parent, (const char*) name, inum);
    if (linked == 0) {
        node->refs += 1;
    }

    return linked;
}

int 
storage_unlink(const char* path)
{
    printf("storage_unlink(%s)\n", path);
    
    char* tmp1 = alloca(strlen(path));
    char* tmp2 = alloca(strlen(path));
    strcpy(tmp1, path);
    strcpy(tmp2, path);

    char* dname = dirname(tmp1);
    char* name = basename(tmp2);
    
    int parent_inum = tree_lookup_inum(dname);
    if (parent_inum <= 0) {
        return -1;
    }

    directory parent_dir = directory_from_inum(parent_inum);
    int inum = directory_delete_ent(parent_dir, (const char*) name);
    if (inum <= 0) {
        return -1;
    }

    inode* node = &(sb->inodes[inum]);

    printf("storage_unlink: sb->inodes[inum].refs = %d\n", node->refs);

    // if there are no more references, clear all the links
    if (--(node->refs) == 0) {
        for (int ii = 0; ii < NUM_BLOCKS; ++ii) {
            int pnum = node->data[ii];
            if (pnum > 2) { // pages 2 and below are vital
                sb->data_map[pnum] = false;
                node->data[ii] = 0;
            }
        }
        node->size = 0;
        sb->inode_map[inum] = false;
    }
    return 0;
}

int 
storage_rmdir(const char* path)
{
    // Return an error if the user tries to remove a non-empty directory
    // The error 'ENOTEMPTY' is probably the right one
    int inum = delete_directory(path);
    if (inum < 0) return inum; 
    if (inum == 0) return -1;

    inode* node = &(sb->inodes[inum]);
    if (--(node->refs) == 0) {
        for (int ii = 0; ii < NUM_BLOCKS; ++ii) {
            int pnum = node->data[ii];
            if (pnum > 2) { // pages 2 and below are vital
                sb->data_map[pnum] = false;
                node->data[ii] = 0;
            }
        }
        node->size = 0;
        sb->inode_map[inum] = false;
    }
    return 0;
}

int 
storage_rename(const char* from, const char* to)
{
    if (storage_link(from, to) < 0) {
       return -1; 
    }

    return storage_unlink(from);
}

int 
storage_chmod(const char* path, mode_t mode)
{
    printf("storage_chmod(%s, %o)\n", path, mode);
    int inum = tree_lookup_inum(path);
    if (inum <= 0) {
        return inum;
    }

    inode* node = &(sb->inodes[inum]);
    node->mode = mode;

    return 0;
}

int 
storage_truncate(const char* path, off_t size)
{
    printf("storage_read(%s)\n", path);
    if (size > 4096) {
        return -1;
    }

    int inum = tree_lookup_inum(path);
    if (inum <= 0) {
        return inum;
    }

    inode* node = &(sb->inodes[inum]);
    node->size = size;

    // TODO: zero out the bytes when expanding a file if you have debris
    // left from shrinking

    return 0;
}

int 
storage_read(const char* path, char* buf, size_t size, off_t offset)
{
    printf("storage_read(%s)\n", path);
    if (offset + size > BLOCK_SIZE) {
        // TODO: implement large files
        return -1;
    }

    int inum = tree_lookup_inum(path);
    if (inum <= 0) {
        return inum;
    }

    inode* node = &(sb->inodes[inum]);

    // TODO loop and get info from node
    int pnum = 0; // FIXME this is a hack to get 'make' to work
    void* page = pages_get_page(pnum);

    int count = clamp(node->size - offset, 0, size);
    memcpy(page + offset, buf, count);
    return count;
}

int 
storage_write(const char* path, const char* buf, size_t size, off_t offset)
{
    printf("storage_write(%s)\n", path);
    if (offset + size > BLOCK_SIZE) {
        // TODO: implement large files
        return -1;
    }

    int inum = tree_lookup_inum(path);
    if (inum <= 0) {
        return inum;
    }

    inode* node = &(sb->inodes[inum]);
    print_node(node);

    return inode_write(node, buf, size, offset);
}

int 
storage_utimens(const char* path, const struct timespec ts[2])
{
    int inum = tree_lookup_inum(path);
    if (inum <= 0) {
        return inum;
    }

    inode* node = &(sb->inodes[inum]);
    node->ctime = ts[0].tv_sec;
    node->mtime = ts[1].tv_sec;

    return 0;
}


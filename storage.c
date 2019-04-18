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
    directory_init(); // init root directory
    s_block = (sp_block *)pages_get_page(0); // point to the beginning of superblock
    printf("storage_init(%s)\n", path);
}

// helper that returns the inode of given path
static inode *
get_node_from_path(const char *path)
{
    int inum = tree_lookup(path); // find the node index in directory
    inode *rv = &(s_block->inodes_start[inum]);
    if (inum <= 0)
    {
        rv = NULL;
    }
    return rv;
}

int storage_stat(const char *path, struct stat *st)
{
    inode *node = get_node_from_path(path);
    if (!node)
    {
        printf("storage_stat(%s)\n", path);
        //printf("returning -ENOENT because given path does not exist!");
        return -ENOENT;
    }
    st->st_size = node->size;
    st->st_mode = node->mode;
    st->st_nlink = node->refs;
    st->st_mtime = node->mtime;
    printf("storage_stat(%s)\n", path);
    return 0;
}

//
int storage_read(const char *path, char *buf, size_t size, off_t offset)
{
    int rv;
    const char* data = storage_data(path);

    rv = strlen(data);
    return rv;
}

int storage_write(const char *path, const char *buf, size_t size, off_t offset)
{
    int rv = -1;
    inode *node = get_node_from_path(path);
    if (!node)
    {
        printf("No such path exists");
        return rv;
    }

    if (size > PAGE_SIZE || node->ptrs[0] == 0)
    {
        printf("Write more than a page? -- not possible yet || empty write?");
        printf("storage_write(%s, %s, %ld, %ld) -> %d\n", path, buf, size, offset, rv);
        return rv;
    }
    void *page_to_write = pages_get_page(node->ptrs[0]);
    if (size + offset < PAGE_SIZE) {
        memcpy(page_to_write, buf, size);
        node->mtime = time(NULL);
        node->size = size; // more work needed
    }
    else {
        return rv;
    }

    rv = size;
    printf("storage_write(%s, %s, %ld, %ld) -> %d\n", path, buf, size, offset, rv);
    return rv;
}

int storage_truncate(const char *path, off_t size)
{
    int rv = -1;
    if (size > PAGE_SIZE)
    {
        printf("truncate() -> size given is larger than page size");
        return rv;
    }

    int prev_size;

    inode *node = get_node_from_path(path);
    if (!node)
    {
        return rv;
    }
    prev_size = node->size;
    node->size = size;
    
    // clear out other data / reevaluate 
    if (prev_size < node->size)
    {
        // fill with \0 -- null bytes
        memset(prev_size + node, 0, node->size - prev_size);

    } else {
        // lost data ==> don't care what happends there or 
    }
    rv = 0; // success
    printf("storage_truncate(%s, %ld)\n", path, size);
    return rv;
}

int storage_mknod(const char *path, mode_t mode)
{
    // work around - got this from stackOverflow
    char *path1 = alloca(strlen(path));
    strcpy(path1, path);
    char *par_dir = dirname(path1);  // parent directory
    directory dir = get_dir_path(par_dir);
    if (!dir.node) // no such directory
    {
        return -ENOENT;
    }

    char *path2 = alloca(strlen(path));
    strcpy(path2, path);    
    char *cur_dir = basename(path2); // curent directory
    if (directory_lookup(dir, cur_dir) != -ENOENT) // file already exist
    {
        return -EEXIST;
    }

    int inum = pages_get_mt_nd();
    if (inum < 0) 
    {
        printf("All inodes are taken");
        return -1;
    }

    inode *node = pages_get_node(inum);
    int mt_page = pages_get_mt_pg();
    init_inode(node, mode);
    node->ptrs[get_mt_db(node)] = mt_page; // point to right db (empty one)

    // update superblock
    s_block->db_map[mt_page] = 1;
    s_block->inodes_map[inum] = 1;
    s_block->inodes_start[inum] = *(node);
    //s_block->db_free_num -= 1;
    //s_block->inodes_num -= 1;

    printf("storage_mknod(%s, %o)\n", path, mode);
    return directory_put(dir, cur_dir, inum);
}

int storage_unlink(const char *path)
{
    int rv = -1;

    // work around -- StOv
    char *path1 = alloca(strlen(path));
    strcpy(path1, path);
    char *par_dir = dirname(path1);
    directory p_dir = get_dir_path(par_dir);

    char *path2 = alloca(strlen(path));
    strcpy(path2, path);
    char *cur_dir = basename(path2);

    int inum = directory_delete(p_dir, (const char *)cur_dir);
    if (inum == -ENOENT)
    {
        printf("could not find dir to delete");
        return rv;
    }

    inode *node = get_inode(inum);


    if (node->refs == 1) {
        int ii = 0;
        while (node->ptrs[ii] > 2 && node->ptrs[ii] != 0 && ii < DIRECT_PTRS) 
        {
            s_block->db_map[node->ptrs[ii]] = 0; // clear db_map
            s_block->db_free_num += 1; // free up
            ++ii;
        }
        // clear node
        node->refs = 0;
        node->mode = 0;
        node->size = 0;
        node->ptrs[0] = 0;
        node->iptr = 0;
        node->ctime = 0;
        node->mtime = 0;
        s_block->inodes_map[inum] = 0; //clear inodes
        s_block->inodes_num += 1; // free up
    }

    // // The only left --> delete
    // if (node->refs == 1)
    // {
    //     for (int ii = 0; ii < DIRECT_PTRS; ++ii)
    //     {
    //         int pnum = node->ptrs[ii];
    //         if (pnum > 2)
    //         {
    //             s_block->db_map[pnum] = 0;
    //             node->ptrs[ii] = 0;
    //         }
    //     }
    //     node->size = 0;
    //     s_block->inodes_map[inum] = 0;
    // }

    rv = 0; //success
    printf("storage_unlink(%s)\n", path);
    return rv;
}

int storage_link(const char *from, const char *to)
{
    int rv = -1;
    inode *node = get_node_from_path(from);
    if (!node)
    {
        return rv;
    }
    // walk around -- from StackOverflow
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

    rv = directory_put(up_one_dir, (const char *)cur_dir, tree_lookup(from));
    if (rv == 0) //success
    {
        node->refs += 1;
    }

    printf("storage_link(%s, %s)\n", from, to);
    return rv;
}

int storage_rename(const char *from, const char *to)
{
    int rv = storage_link(from, to);
    if (rv == 0) // success
    {
        rv = storage_unlink(from);
    }

    printf("storage_rename(%s, %s)\n", from, to);
    return rv;
}

int storage_set_time(const char *path, const struct timespec ts[2])
{
    int rv = -1;
    inode *node = get_node_from_path(path);
    if (!node)
    {
        return rv;
    }
    node->ctime = ts[0].tv_sec;
    node->mtime = ts[1].tv_sec;

    rv = 0; // success
    printf("storage_set_time(%s)\n", path);
    return rv;
}

// get data from db
const char *
storage_data(const char *path)
{
    inode *node = get_node_from_path(path);
    if (!node)
    {
        return NULL; // no node found
    }

    // char* data = 0;
    // for (int ii = 0; ii < DIRECT_PTRS; ++ii)
    // {
    //     int pnum = node->ptrs[ii];
    //     if (pnum == 0)
    //     {
    //         break;
    //     }

    //     data = strcat(data, (const char *)pages_get_page(pnum));
    //     printf("storage_data(%s) -> %s\n", path, data);
    //     // return data;
    // }
    // return (const char*)data;

    int pnum = node->ptrs[0];
    if (pnum == 0)
    {
        return NULL; // EMPTY first db
    }

    const char *data = (const char *)pages_get_page(pnum);
    printf("storage_data(%s) -> %s\n", path, data);
    return data;
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

    directory dir = get_dir_path(par_dir);

    // init pointers
    int pnum = pages_get_mt_pg();
    int inum = pages_get_mt_nd();
    inode *node = &(s_block->inodes_start[inum]);
    mode_t dir_mode = (mode |= 040000);
    init_inode(node, dir_mode);
    node->ptrs[get_mt_db(node)] = pnum;
    node->size += PAGE_SIZE;

    // update superblock --> to taken
    s_block->inodes_map[inum] = 1;
    s_block->db_map[pnum] = 1;

    printf("storage_mkdir(%s, %o)\n", path, mode);
    return directory_put(dir, cur_dir, inum);
}

int storage_rmdir(const char *path)
{
    int rv = -1;

    // cannot delete nonempty dir
    int inum = rm_dir(path);
    if (inum <= 0)
    {
        return rv;
    }

    inode *node = &(s_block->inodes_start[inum]);
    // The only left --> delete
    if (node->refs == 1)
    {
        for (int ii = 0; ii < DIRECT_PTRS; ++ii)
        {
            int pnum = node->ptrs[ii];
            if (pnum > 2)
            {
                // printf("db_map[pnum = %d] = $d\n", pnum, s_block->db_map[pnum]);
                // printf("inode->ptrs[ii = %d] = %d\n", ii, node->ptrs[ii]);
                s_block->db_map[pnum] = 0;
                node->ptrs[ii] = 0;
                // puts("DELETED -- remove all drct");
                // printf("db_map[pnum = %d] = $d\n", pnum, s_block->db_map[pnum]);
                // printf("inode->ptrs[ii = %d] = %d\n", ii, node->ptrs[ii]);
            }
        }
        node->size = 0;
        s_block->inodes_map[inum] = 0;
    }

    rv = 0; // success
    printf("storage_rmdir(%s)\n", path);
    return rv;
}

int storage_chmod(const char *path, mode_t mode)
{
    int rv = -1;

    inode *node = get_node_from_path(path);
    if (!node)
    {
        return rv;
    }
    node->mode = mode;

    rv = 0; // success
    printf("storage_chmod(%s, %o)\n", path, mode);
    return rv;
}

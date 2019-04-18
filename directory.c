#include <stdbool.h>
#include <alloca.h>
#include <libgen.h>
#include <errno.h>
#include <stdlib.h>

#include "slist.h"
#include "pages.h"
#include "directory.h"
#include "util.h"

// typedef struct dirent
// {
//     char name[DIR_NAME];
//     int inum;
//     char _reserved[12];
// } dirent;

// typedef struct directory
// {
//     int pnum;
//     inode *node;
//     dirent *dirents;
// } directory;

static int root_dir = 1; // Root dir inode

// Initialize directory
void directory_init()
{
    inode *root_node = pages_get_node(root_dir); // get the root inode

    // Root directory initialized?
    if (root_node->refs == 0)
    {
        init_inode(root_node, 040755);
        inode_set_ptrs(root_node, 2, PAGE_SIZE);
    }
    printf("directory_init()\n");
}

int directory_lookup(directory dd, const char *name)
{
    int rv = -ENOENT;

    for (int ii = 0; ii < DIR_LIMIT; ++ii)
    {
        if (streq(dd.dirents[ii].name, name))
        {
            rv = dd.dirents[ii].inum;
            break;
        }
    }

    printf("directory_lookup(dd, %s) -> %d\n", name, rv);
    return rv;
}

int tree_lookup(const char *path)
{
    int rv = -ENOENT;
    // only works with absolute path
    slist *dirs = directory_list(path);
    directory dd;

    if (!streq(dirs->data, ""))
    {
        dprintf(2, "tree_lookup. Dir doesn't start with root\n");
        return -1;
    }

    dd = get_dir_inum(root_dir);
    int inum = root_dir;
    bool found_flag;
    dirs = dirs->next;

    while (dirs != 0)
    {
        printf("in loop, dirs->data: %s\n", dirs->data);
        found_flag = 0;
        for (int ii = 0; ii < DIR_LIMIT; ++ii)
        {
            if (streq(dd.dirents[ii].name, dirs->data))
            {
                inum = dd.dirents[ii].inum;
                inode *node = pages_get_node(inum);
                if (node->mode & 040000)
                {
                    dd = get_dir_inum(inum);
                }
                found_flag = 1;
                break;
            }
        }
        if (!found_flag)
        {
            printf("tree_lookup(%s) -> %d\n", path, rv);
            return rv; // file not found
        }
        dirs = dirs->next;
    }

    rv = inum;
    printf("tree_lookup(%s) -> %d\n", path, rv);

    return rv;
}

int directory_put(directory dd, const char *name, int inum)
{
    int rv = -1;
    for (int ii = 0; ii < DIR_LIMIT; ++ii)
    {
        if (dd.dirents[ii].inum == 0)
        {
            strcpy(dd.dirents[ii].name, name);
            dd.dirents[ii].inum = inum;
            rv = 0;
            break;
        }
    }

    printf("directory_put(dd, %s, %d) -> %d\n", name, inum, rv);
    return rv;
}

int directory_delete(directory dd, const char *name)
{
    int rv = -ENOENT;
    for (int ii = 0; ii < DIR_LIMIT; ++ii)
    {
        if (streq(dd.dirents[ii].name, name))
        {
            int inum = dd.dirents[ii].inum;
            dd.dirents[ii].name[0] = 0;
            dd.dirents[ii].inum = 0;
            // dd.dirents[ii] = 0;
            rv = inum;
            break;
        }
    }

    printf("directory_delete(dd, %s) -> %d\n", name, rv);
    return rv;
}

slist *directory_list(const char *path)
{
    return s_split(path, '/');
}

directory get_dir_inum(int inum)
{
    inode *node = pages_get_node(inum);
    print_node(node);

    dirent *entries = (dirent *)pages_get_page(node->ptrs[0]);
    directory dir;
    dir.pnum = node->ptrs[0];
    dir.dirents = entries;
    dir.node = node;

    printf("get_dir_inum(%d)\n", inum);
    return dir;
}

directory get_dir_path(const char *path)
{
    int inum = tree_lookup(path);
    if (inum == -ENOENT)
    {
        directory empty; // TODO: change this return
        return empty;
    }

    printf("get_dir_path(%s)\n", path);
    return get_dir_inum(inum);
}

// return reomved dir's inode index
int rm_dir(const char *path)
{

    int rv = -ENOENT;

    char *path1 = alloca(strlen(path));
    char *path2 = alloca(strlen(path));
    strcpy(path1, path);
    strcpy(path2, path);
    char *par_dir = dirname(path1);
    char *cur_dir = basename(path2);

    int p_inum = tree_lookup(par_dir);
    if (p_inum <= 0)
    {
        rv = p_inum;
        printf("rm_dir(%s) -> %d\n", path, rv);
        return rv;
    }

    directory p_dir = get_dir_inum(p_inum);
    dirent *c_ent = 0;
    for (int ii = 0; ii < DIR_LIMIT; ++ii)
    {
        if (streq(p_dir.dirents[ii].name, cur_dir))
        {
            c_ent = &(p_dir.dirents[ii]);
            break;
        }
    }

    if (c_ent == 0)
    {
        printf("rm_dir(%s) -> %d\n", path, rv);
        return rv;
    }

    directory c_dir = get_dir_inum(c_ent->inum);
    if (c_dir.pnum == 0)
    {
        printf("rm_dir(%s) -> %d\n", path, rv);
        return rv;
    }

    for (int ii = 0; ii < DIR_LIMIT; ++ii)
    {
        if (c_dir.dirents[ii].name[0] != 0)
        {
            rv = -ENOTEMPTY;
            printf("rm_dir(%s) -> %d\n", path, rv);
            return rv;
        }
    }

    c_dir.pnum = 0;
    c_dir.node = 0;

    int inum = c_ent->inum;
    c_ent->inum = 0;
    c_ent->name[0] = 0;

    rv = inum;
    printf("rm_dir(%s) -> %d\n", path, rv);
    return rv;
}

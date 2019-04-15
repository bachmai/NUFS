#include <stdbool.h>
#include <errno.h>
#include <alloca.h>
#include <libgen.h>

#include "slist.h"
#include "pages.h"
#include "directory.h"
#include "util.h"

// typedef struct dirent {
//     char name[DIR_NAME];
//     int  inum;
//     char _reserved[12];
// } dirent;
// TODO

static int root = 1; // Root inode number

void directory_init()
{
    inode* root_dir = pages_get_node(root);
    if (root_dir->refs == 0) {
        inode_init(root_dir, 040755);
        inode_set_data(root_dir, 2, BLOCK_SIZE);
    }
}

// FIXME this returns inode num, not pnum
int directory_lookup(directory dd, const char* name)
{
    printf("directory_lookup(dd, %s)\n", name);
    for (int ii = 0; ii < DIR_SIZE; ++ii) {
        if (streq(dd.ents[ii].name, name)) {
            return dd.ents[ii].node_idx;
        }
    }

    // file not found
    return -ENOENT;
}

int tree_lookup(const char* path)
{
    printf("tree_lookup(%s)\n", path);
    // Assumes absolute paths
    slist* dirs = directory_list(path);
    directory dd;

    if (!streq(dirs->data, "")) {
        dprintf(2, "Improper path\n");
        return -1; // break if the first item isn't root
    }
    
    dd = get_dir_inum(root);
    int inum = root;
    bool found_flag;
    dirs = dirs->next; // we have root, so advance the list

    while (dirs != 0) {
        printf("in loop, dirs->data: %s\n", dirs->data);
        found_flag = false;
        for (int ii = 0; ii < DIR_SIZE; ++ii) {
            if (streq(dd.ents[ii].name, dirs->data)) {
                inum = dd.ents[ii].node_idx;
                inode* node = pages_get_node(inum);
                if (node->mode & 040000) {
                    dd = get_dir_inum(inum);
                }
                found_flag = true;
                break;
            }
        }
        if (!found_flag) {
            return -ENOENT;
        }
        dirs = dirs->next;
    }

    return inum;
}

// void print_directory(directory dd);

int directory_put(directory dd, const char* name, int inum)
{
    printf("directory_put(dd, %s, %d)\n", name, inum);
    for (int ii = 0; ii < DIR_SIZE; ++ii) {
        if (dd.ents[ii].node_idx == 0) {
            strcpy(dd.ents[ii].name, name);
            dd.ents[ii].node_idx = inum;
            return 0;
        }
    }
    return -1;
}

// return the node index of the deleted item on success
int directory_delete(directory dd, const char* name)
{
    for (int ii = 0; ii < DIR_SIZE; ++ii) {
        if (streq(dd.ents[ii].name, name)) {
            int inum = dd.ents[ii].node_idx;
            dd.ents[ii].name[0] = 0;
            dd.ents[ii].node_idx = 0;
            // dd.ents[ii] = 0;
            return inum;
        }
    }

    return -ENOENT;
}

slist* directory_list(const char* path)
{
    return s_split(path, '/');
}

directory get_dir_inum(int inum)
{
    printf("get_dir_inum(%d)\n", inum);
    inode* node = pages_get_node(inum);
    print_node(node);
    
    dirent* entries = (dirent*) pages_get_page(node->data[0]);
    directory dir;
    dir.pnum = node->data[0];
    dir.ents = entries;
    dir.node = node; 

    return dir;
}

directory get_dir_path(const char* path)
{
    printf("get_dir_path(%s)\n", path);

    int inum = tree_lookup(path);
    if (inum == -ENOENT) {
        directory dd; // maybe do something smarter here
        return dd;
    }

    return get_dir_inum(inum);
}

// returns the inode number of the deleted directory on success
int rm_dir(const char* path)
{
    char* tmp1 = alloca(strlen(path));
    char* tmp2 = alloca(strlen(path));
    strcpy(tmp1, path);
    strcpy(tmp2, path);

    char* dname = dirname(tmp1);
    char* name = basename(tmp2);
    
    int parent_inum = tree_lookup(dname);
    if (parent_inum <= 0) {
        return parent_inum;
    }

    directory parent_dir = get_dir_inum(parent_inum);
    dirent* child_ent = 0;
    for (int ii = 0; ii < DIR_SIZE; ++ii) {
        if (streq(parent_dir.ents[ii].name, name)) {
            child_ent = &(parent_dir.ents[ii]);
            break;
        }
    }

    if (child_ent == 0) {
        return -ENOENT;
    }

    directory child = get_dir_inum(child_ent->node_idx);
    if (child.pnum == 0) {
        return -ENOENT;
    }

    for (int ii = 0; ii < DIR_SIZE; ++ii) {
        if (child.ents[ii].name[0] != 0) {
            return -ENOTEMPTY;
        }
    }
    
    child.pnum = 0;
    child.node = 0;

    int inum = child_ent->node_idx;
    child_ent->node_idx = 0;
    child_ent->name[0] = 0;

    return inum;
}


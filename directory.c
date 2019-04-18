#include <stdbool.h>
#include <alloca.h>
#include <libgen.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

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

// Initialize directory-root
void directory_init()
{
    inode *root_node = pages_get_node(1); // get inode page

    // Root directory initialization if not
    if (root_node->refs == 0)
    {
        root_node->refs = 1;
        root_node->mode = 040755;
        root_node->ptrs[0] = 2; // db
        root_node->iptr = 0;
        root_node->size = PAGE_SIZE;
        root_node->ctime = time(NULL);
        root_node->mtime = time(NULL);
    }
    // else {
    //     root_node->ptrs[0] = pages_get_mt_pg; // reinitialize db?
    //     root_node->ctime = time(NULL);
    //     root_node->mtime = time(NULL);
    // }
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

// helper to find dirent that matches the name
dirent*
find_dirent_name(directory dd, const char* name)
{
    int ii = 0;
    while (ii < DIR_LIMIT && !streq(dd.dirents[ii].name, name))
    {
        // Until reach limit 
        ++ii;
    }

    if (ii == DIR_LIMIT) {
        return NULL; // not found, but caller should check before call
    }

    return dd.dirents + ii;
}

int tree_lookup(const char *path)
{
    int rv = 1;
    
    directory dd = get_dir_inum(1); // get root directory
    slist *dirs = directory_list(path); // dirs slist
    dirs = dirs->next;
    dirent* dirent;
    // Loop through slist dirs checking if path is in the root directory dir by dir
    while (dirs != 0) {
        printf("current dir->data in dirs list is %s\n", dirs->data);
        dirent = find_dirent_name(dd, dirs->data);
        if (dirent) 
        {
            // NOT NULL
            rv = dirent->inum;
            // change directory --> next one
            inode* node = pages_get_node(rv);   // get the inode
            if (node->mode & 040000) { // Bitwise is used to check for the type of file -->> directory in this case
                dd = get_dir_inum(rv);
            }
            // keep traversing
            dirs = dirs->next;
        }
        else 
        {
            // NULLl -> not found
            rv = -ENOENT;
            printf("tree_lookup(%s) -> %d\n", path, rv);
            return rv;
        }
    }
    printf("tree_lookup(%s) -> %d\n", path, rv);
    return rv; // -- inode number
}

// index of free inode
int 
find_inum_mt(dirent* dirents) 
{
    int rv = 0;
    while (rv < DIR_LIMIT && dirents[rv].inum != 0){
        // Inside the limits and not empty
        ++rv;
    }
    return rv;
}

int directory_put(directory dd, const char *name, int inum)
{
    int rv = -1;

    int idx = find_inum_mt(&dd.dirents[0]);
    if (idx < DIR_LIMIT) {
        dd.dirents[idx].inum = inum;
        strcpy(dd.dirents[idx].name, name);
        rv = 0; //success
    }

    printf("directory_put(dd, %s, %d) -> %d\n", name, inum, rv);
    return rv;
}

int directory_delete(directory dd, const char *name)
{
    int rv = directory_lookup(dd, name);
    // ensure there is such dir
    if (rv != -ENOENT)
    {
        dirent* entry = find_dirent_name(dd, name);
        rv = entry->inum;
        // clear mapping
        entry->name[0] = 0;
        entry->inum = 0;
        entry = 0;
    }

    printf("directory_delete(dd, %s) -> %d\n", name, rv);
    return rv;
}

slist *directory_list(const char *path)
{
    return s_split(path, '/');
}

// get directory given the inum
directory get_dir_inum(int inum)
{
    directory rv;
    inode *node = pages_get_node(inum);
    int pnum = node->ptrs[0]; // page number first
    // directory db
    rv.pnum = pnum;
    rv.dirents = (dirent *)pages_get_page(pnum);
    rv.node = node;
    
    printf("get_dir_inum(%d)\n", inum);
    return rv;
}

// given the path
directory get_dir_path(const char *path)
{
    // Find inode number -> directory
    printf("get_dir_path(%s)\n", path);
    return get_dir_inum(tree_lookup(path));
}

// return reomved dir's inode index
int rm_dir(const char *path)
{
    int rv = -ENOENT;
    char path1[strlen(path)];
    strcpy(path1, path);
    char *par_dir = dirname(path1); // parent dir
    // Check for parent node index
    if (tree_lookup(par_dir) <= 0)
    {
        //printf("PARENT");
        printf("rm_dir(%s) -> %d\n", path, rv);
        return rv;
    }
    directory p_dir = get_dir_path(par_dir);    // directory
    char path2[strlen(path)];
    strcpy(path2, path);
    char *cur_dir = basename(path2);    // current dirr
    if (tree_lookup(cur_dir) <= 0)
    {
        //printf("ME");
        printf("rm_dir(%s) -> %d\n", path, rv);
        return rv;
    }
    // dirent *c_ent = find_
    dirent *c_ent = find_dirent_name(p_dir,cur_dir);
    rv = c_ent->inum;

    directory c_dir = get_dir_path(cur_dir);
    // ensure dirents are empty
    for (int ii = 0; ii < DIR_LIMIT; ++ii)
    {
        if (c_dir.dirents[ii].inum != 0 && !streq(c_dir.dirents[ii].name, 0))
        {
            rv = -ENOTEMPTY;  // not empty --> cannot delete
            break;
        }
    }
    
    printf("rm_dir(%s) -> %d\n", path, rv);
    return rv;
}
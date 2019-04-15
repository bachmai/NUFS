// based on cs3650 starter code

#ifndef DIRECTORY_H
#define DIRECTORY_H

#define DIR_NAME 48

#define DIR_LIMIT 64

#include "slist.h"
#include "pages.h"
// #include "inode.h"

typedef struct dirent
{
    char name[DIR_NAME];
    int inum;
    char _reserved[12];  // what to do with this?
} dirent;

typedef struct directory
{
    int pnum;
    inode *node;
    dirent *dirents;
} directory;

void directory_init();
int directory_lookup(directory dd, const char* name);
int tree_lookup(const char* path);
int directory_put(directory dd, const char* name, int inum);
int directory_delete(directory dd, const char* name);
slist* directory_list(const char* path);
void print_directory(directory dd);

directory get_dir_inum(int inum);
directory get_dir_path(const char *path);
int rm_dir(const char *path);

#endif


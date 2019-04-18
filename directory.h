// based on cs3650 starter code

#ifndef DIRECTORY_H
#define DIRECTORY_H

#define DIR_NAME 36
#define DIR_LIMIT 64   // 8inodes x 8db

#include "slist.h"
#include "pages.h"

typedef struct dirent
{
    char name[DIR_NAME];
    int inum;           // inode number
    char _reserved[12]; // do we actuualy need?
} dirent;

typedef struct directory
{
    int pnum;
    dirent *dirents; // array of dirents
    inode *node;     // pointer to inode     
} directory;

void directory_init();
int directory_lookup(directory dd, const char *name);
int tree_lookup(const char *path);
int directory_put(directory dd, const char *name, int inum);
int directory_delete(directory dd, const char *name);
slist *directory_list(const char *path);
void print_directory(directory dd);
directory get_dir_inum(int inum);
directory get_dir_path(const char *path);
int rm_dir(const char *path);

#endif

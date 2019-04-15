// based on cs3650 starter code

#ifndef DIRECTORY_H
#define DIRECTORY_H

#define DIR_NAME 48

#define DIR_SIZE 64

#include "slist.h"
#include "pages.h"
// #include "inode.h"

// typedef struct dirent {
//     char name[DIR_NAME];
//     int  inum;
//     char _reserved[12];
// } dirent;

typedef struct dirent
{
    char name[DIR_NAME];
    int node_idx;
} dirent;

typedef struct directory
{
    int pnum;
    dirent *ents;
    inode *node;
} directory;

// void directory_init();
// int directory_lookup(inode* dd, const char* name);
// int tree_lookup(const char* path);
// int directory_put(inode* dd, const char* name, int inum);
// int directory_delete(inode* dd, const char* name);
// slist* directory_list(const char* path);
// void print_directory(inode* dd);

void directory_init();
directory directory_from_inum(int inum);
int directory_lookup_inum(directory dd, const char *name);
int tree_lookup_inum(const char *path);
directory directory_from_path(const char *path);
int directory_put_ent(directory dd, const char *name, int inum);
int directory_delete_ent(directory dd, const char *name);
int delete_directory(const char *path);
slist *directory_list(const char *path);
void print_directory(directory dd);

#endif


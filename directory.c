#include "directory.h"

// typedef struct dirent {
//     char name[DIR_NAME];
//     int  inum;
//     char _reserved[12];
// } dirent;

// TODO
void directory_init();
int directory_lookup(inode* dd, const char* name);
int tree_lookup(const char* path);
int directory_put(inode* dd, const char* name, int inum);
int directory_delete(inode* dd, const char* name);
slist* directory_list(const char* path);
void print_directory(inode* dd);
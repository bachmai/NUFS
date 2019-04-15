// based on cs3650 starter code

#ifndef NUFS_STORAGE_H
#define NUFS_STORAGE_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fuse.h>

#include "slist.h"

void storage_init(const char* path);
int get_stat(const char* path, struct stat* st);
const char* get_data(const char* path);

int storage_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset);
int storage_mknod(const char* path, mode_t mode);
int storage_mkdir(const char* path, mode_t mode);
int storage_link(const char* from, const char* to);
int storage_unlink(const char* path);
int storage_rmdir(const char* path);
int storage_rename(const char* from, const char* to);
int storage_chmod(const char* path, mode_t mode);
int storage_truncate(const char* path, off_t size);
int storage_read(const char* path, char* buf, size_t size, off_t offset);
int storage_write(const char* path, const char* buf, size_t size, off_t offset);
int storage_utimens(const char* path, const struct timespec ts[2]);


#endif

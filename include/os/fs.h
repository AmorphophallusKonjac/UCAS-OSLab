#ifndef __FS_H__
#define __FS_H__

#include <type.h>
#include <os/lock.h>

#define FS_SECTOR_BASE (1ul << 20)

#define DISK_CACHE_SIZE (1ul << 22)

#define DISK_CACHE_BASE 0xffffffc058000000ul

#define DISK_BLOCK_SIZE (1ul << 12)

#define DISK_CACHE_NUM (DISK_CACHE_SIZE / DISK_BLOCK_SIZE)

#define SECTOR_SIZE 512

#define FS_MAGIC 0x19260817

#define BLOCK_MAP_SIZE 0x6000

#define INODE_MAP_SIZE 0x4000

#define DATA_PTR_NUM 13

#define DENTRY_NAME_SIZE 124

#define BUF_SIZE 512

#define FILE 0

#define DIR 1

#define FS_SIZE (1ul << 29)

#define FD_ACCESS_READ 1

#define FD_ACCESS_WRITE 2

#define FD_NUM 100

#define SEEK_SET 0

#define SEEK_CUR 1

#define SEEK_END 2

char *disk_cache_data;

typedef struct disk_cache_v_idx {
	uint32_t sector_idx, v;
} disk_cache_v_idx_t;

uint32_t disk_cache_used;

typedef struct fs {
	uint64_t magic;
	uint32_t block_map_offset, block_map_size;
	uint32_t inode_map_offset, inode_map_size;
	uint32_t inode_offset, inode_size;
	uint32_t data_offset, data_size;
	uint32_t size;
} fs_t;

fs_t fs;

uint32_t inum;

typedef struct inode {
	uint16_t mode;
	uint16_t link_num;
	uint32_t size;
	uint32_t inum;
	uint32_t data_ptr[DATA_PTR_NUM];
} inode_t;

typedef struct dentry {
	uint32_t inum;
	char name[DENTRY_NAME_SIZE];
} dentry_t;

typedef struct axis {
	uint32_t axis[4];
} axis_t;

typedef struct fd {
	uint32_t access;
	uint32_t pos;
	uint32_t inum;
	uint32_t pid;
	spin_lock_t lock;
} fd_t;

void init_map();
void init_inode();
void print_superblock(fs_t *fs);
void do_mkfs();
void init_disk_cache();
void local_flush_disk_cache();
char *read_disk_block(uint32_t offset);
void read_disk(uint32_t offset, char *data, int len);
void write_disk(uint32_t offset, char *data, int len);
uint32_t lookup_disk_cache(uint32_t sector_idx);
uint32_t refill_disk_cache(uint32_t sector_idx);
uint32_t internel_mkdir(uint32_t fa_inum, char *name);
uint32_t alloc_inode();
void free_inode(uint32_t inode_offset);
void add_dentry(uint32_t inode_offset, uint32_t inum, char *name);
void del_dentry(uint32_t inode_offset, uint32_t inum);
uint32_t get_inum(uint32_t inode_offset);
void parse_inode_ptr(axis_t *pos, uint64_t ptr);
uint32_t get_block(inode_t *inode, uint32_t inode_size, uint32_t ptr);
uint32_t alloc_block();
void free_block(uint32_t block_offset);
void free_inode_block(uint32_t inode_offset);
uint32_t get_inode_offset(uint32_t inum);
void do_ls(char *path, int detailed);
uint32_t internel_ls(uint32_t dir_inum, int detailed);
void do_mkdir(char *path);
void parse_path(char *path, uint32_t *fa_inum, uint32_t *inum, uint16_t mode,
		char **file_name);
uint32_t find_file(uint32_t inum, char *name, uint16_t mode);
void do_statfs();
void do_cd(char *path);
void internel_cd(char *wd, char *path, uint32_t *wd_inum);
void normalize_path(char *path);
void do_rwd(char *wd);
void do_rmdir(char *path);
void internel_rmfile(uint32_t dir_inum, uint32_t fa_dir_inum);
void do_touch(char *path);
void internel_touch(uint32_t fa_inum, char *name);
void do_cat(char *path);
void internel_cat(uint32_t inum);
void do_ln(char *link_target, char *path);
void internel_ln(uint32_t dir_inum, uint32_t target_inum, char *name);
void do_rm(char *path);
void init_fd();
int do_fopen(char *path, uint32_t access);
int internel_fopen(uint32_t inum, uint32_t access);
void do_fclose(int fd);
int do_fread(int fd_num, char *buff, int size);
int do_fwrite(int fd_num, char *buff, int size);
int do_lseek(int fd_num, int offset, int whence);

#endif
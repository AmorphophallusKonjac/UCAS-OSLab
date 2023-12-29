#ifndef __FS_H__
#define __FS_H__

#include <type.h>

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
void add_dentry(uint32_t inode_offset, uint32_t inum, char *name);
uint32_t get_inum(uint32_t inode_offset);
void parse_inode_ptr(axis_t *pos, uint64_t ptr);
uint32_t get_block(inode_t *inode, uint32_t inode_size, uint32_t ptr);
uint32_t alloc_block();
uint32_t get_inode_offset(uint32_t inum);
void do_ls(uint32_t inum, char *path, int detailed);
uint32_t internel_ls(uint32_t inum, int detailed);
void do_mkdir(uint32_t inum, char *path);
void parse_path(uint32_t dir_inum, char *path, uint32_t *fa_inum,
		uint32_t *inum, uint16_t mode, char **file_name);
uint32_t find_file(uint32_t inum, char *name, uint16_t mode);

#endif
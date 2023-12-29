#include <os/fs.h>
#include <os/string.h>
#include <os/kernel.h>
#include <printk.h>
#include <screen.h>

disk_cache_v_idx_t disk_cache_v_idx[DISK_CACHE_NUM];

void print_superblock(fs_t *fs)
{
	printk("[FS] magic : 0x%lX\n", fs->magic);
	printk("[FS] size : 0x%lXMB\n", fs->size / 1024 / 1024);
	printk("[FS] block map offset : 0x%lX\n", fs->block_map_offset);
	printk("[FS] inode map offset : 0x%lX\n", fs->inode_map_offset);
	printk("[FS] inode offset : 0x%lX\n", fs->inode_offset);
	printk("[FS] data offset : 0x%lX\n", fs->data_offset);
	printk("[FS] inode size : 0x%lXB, dir entry size : 0x%lXB\n",
	       sizeof(inode_t));
}

void init_map(uint32_t offset, uint32_t size)
{
	for (int i = 0; i < size; i += DISK_BLOCK_SIZE) {
		write_disk(offset + i, 0, DISK_BLOCK_SIZE);
	}
}

void init_inode()
{
	init_map(fs.inode_offset, fs.inode_size);
	internel_mkdir(0, "");
}

void do_mkfs()
{
	screen_move_cursor(0, 3);
	printk("[FS] Start filesystem!\n");
	char *super_block = read_disk_block(0);
	memcpy((uint8_t *)&fs, (uint8_t *)super_block, sizeof(fs_t));
	if (fs.magic == FS_MAGIC) {
		printk("[FS] Getting superblock...\n");
		print_superblock(&fs);
	} else {
		printk("[FS] Can not find filesystem, create new one!\n");
		printk("[FS] Setting superblock...\n");
		fs.magic = FS_MAGIC;
		fs.size = FS_SIZE;

		fs.block_map_offset = DISK_BLOCK_SIZE;
		fs.block_map_size = BLOCK_MAP_SIZE;

		fs.inode_map_offset = fs.block_map_offset + fs.block_map_size;
		fs.inode_map_size = INODE_MAP_SIZE;

		fs.inode_offset = fs.inode_map_offset + fs.inode_map_size;
		fs.inode_size = sizeof(inode_t) * 8 * fs.inode_map_size;

		fs.data_offset = fs.inode_offset + fs.inode_size;
		fs.data_size = DISK_BLOCK_SIZE * 8 * fs.block_map_size;

		write_disk(0, (char *)&fs, sizeof(fs_t));
		print_superblock(&fs);

		printk("[FS] Setting block map...\n");

		init_map(fs.block_map_offset, fs.block_map_size);

		printk("[FS] Setting inode map...\n");

		init_map(fs.inode_map_offset, fs.inode_map_size);

		printk("[FS] Setting inode...\n");

		init_inode();
	}
	printk("[FS] finished!\n");
}

void init_disk_cache()
{
	disk_cache_data = (char *)DISK_CACHE_BASE;
	bzero((void *)disk_cache_data, DISK_CACHE_SIZE);
	for (int i = 0; i < DISK_CACHE_NUM; ++i) {
		disk_cache_v_idx[i].sector_idx = 0;
		disk_cache_v_idx[i].v = 0;
	}
	disk_cache_used = 0;
}

void local_flush_disk_cache()
{
	init_disk_cache();
}

uint32_t lookup_disk_cache(uint32_t sector_idx)
{
	for (int i = 0; i < DISK_CACHE_NUM; ++i) {
		if (disk_cache_v_idx[i].v == 1 &&
		    disk_cache_v_idx[i].sector_idx == sector_idx) {
			return i;
		}
	}
	return -1;
}

uint32_t refill_disk_cache(uint32_t sector_idx)
{
	static uint32_t i = 0;
	if (disk_cache_used < DISK_CACHE_NUM) {
		for (;;) {
			if (disk_cache_v_idx[i].v == 0) {
				break;
			}
			i = (i + 1) % DISK_CACHE_NUM;
		}
		++disk_cache_used;
	}
	uint32_t ret = i;
	i = (i + 1) % DISK_CACHE_NUM;
	uintptr_t mem_addr = DISK_CACHE_BASE + DISK_BLOCK_SIZE * ret;
	bios_sd_read(kva2pa(mem_addr), DISK_BLOCK_SIZE / SECTOR_SIZE,
		     sector_idx);
	disk_cache_v_idx[ret].v = 1;
	disk_cache_v_idx[ret].sector_idx = sector_idx;
	return ret;
}

char *read_disk_block(uint32_t offset)
{
	uint32_t sector_idx = FS_SECTOR_BASE + offset / SECTOR_SIZE;
	sector_idx =
		sector_idx - (sector_idx % (DISK_BLOCK_SIZE / SECTOR_SIZE));
	int disk_cache_idx = lookup_disk_cache(sector_idx);
	if (disk_cache_idx == -1) { // miss
		disk_cache_idx = refill_disk_cache(sector_idx);
	}
	return (char *)DISK_CACHE_BASE + (disk_cache_idx * DISK_BLOCK_SIZE);
}

void read_disk(uint32_t offset, char *data, int len)
{
	char *buf = read_disk_block(offset);
	uint32_t block_offset = offset % DISK_BLOCK_SIZE;
	memcpy((void *)data, (void *)buf + block_offset, len);
}

void write_disk(uint32_t offset, char *data, int len)
{
	uint32_t sector_idx = FS_SECTOR_BASE + offset / SECTOR_SIZE;
	sector_idx =
		sector_idx - (sector_idx % (DISK_BLOCK_SIZE / SECTOR_SIZE));
	char *buf = read_disk_block(offset);
	uint32_t block_offset = offset % DISK_BLOCK_SIZE;
	if (data != 0) {
		memcpy((void *)buf + block_offset, (void *)data, len);
	} else {
		bzero(buf + block_offset, len);
	}
	bios_sd_write(kva2pa((uintptr_t)buf), DISK_BLOCK_SIZE / SECTOR_SIZE,
		      sector_idx);
}

uint32_t alloc_inode()
{
	inode_t inode;
	inode.mode = 0;
	inode.link_num = 1;
	inode.size = 0;
	inode.inum = ++inum;
	for (int i = 0; i < DATA_PTR_NUM; ++i) {
		inode.data_ptr[i] = 0;
	}
	uint32_t ret = 0;
	for (int i = 0; i < fs.inode_map_size; i += DISK_BLOCK_SIZE) {
		char *inode_map_buf = read_disk_block(fs.inode_map_offset + i);
		for (int j = 0; j < DISK_BLOCK_SIZE; ++j) {
			for (int k = 0; k < 8; ++k) {
				if ((inode_map_buf[j] & (1 << k)) == 0) {
					char data = inode_map_buf[j] | (1 << k);
					write_disk(fs.inode_map_offset + i + j,
						   &data, 1);
					uint32_t inode_idx =
						(i * DISK_BLOCK_SIZE + j) * 8 +
						k;
					ret = fs.inode_offset +
					      inode_idx * sizeof(inode_t);
					write_disk(ret, (char *)&inode,
						   sizeof(inode_t));
					return ret;
				}
			}
		}
	}
	return ret;
}

void parse_inode_ptr(axis_t *pos, uint64_t ptr)
{
	if (ptr < 7 * DISK_BLOCK_SIZE) {
		pos->axis[0] = ptr / DISK_BLOCK_SIZE;
		pos->axis[1] = 0;
		pos->axis[2] = 0;
		pos->axis[3] = 0;
		return;
	}

	ptr -= 7 * DISK_BLOCK_SIZE;

	if (ptr < 3ll * DISK_BLOCK_SIZE * DISK_BLOCK_SIZE) {
		pos->axis[0] = ptr / (DISK_BLOCK_SIZE * DISK_BLOCK_SIZE) + 7;
		ptr %= DISK_BLOCK_SIZE * DISK_BLOCK_SIZE;
		pos->axis[1] = ptr / DISK_BLOCK_SIZE;
		pos->axis[2] = 0;
		pos->axis[3] = 0;
		return;
	}

	ptr -= 3 * DISK_BLOCK_SIZE * DISK_BLOCK_SIZE;

	if (ptr < 2ll * DISK_BLOCK_SIZE * DISK_BLOCK_SIZE * DISK_BLOCK_SIZE) {
		pos->axis[0] = ptr / (DISK_BLOCK_SIZE * DISK_BLOCK_SIZE *
				      DISK_BLOCK_SIZE) +
			       10;
		ptr %= DISK_BLOCK_SIZE * DISK_BLOCK_SIZE * DISK_BLOCK_SIZE;
		pos->axis[1] = ptr / (DISK_BLOCK_SIZE * DISK_BLOCK_SIZE);
		ptr %= DISK_BLOCK_SIZE * DISK_BLOCK_SIZE;
		pos->axis[2] = ptr / DISK_BLOCK_SIZE;
		pos->axis[3] = 0;
		return;
	}

	ptr -= 2 * DISK_BLOCK_SIZE * DISK_BLOCK_SIZE * DISK_BLOCK_SIZE;

	if (ptr < 1ll * DISK_BLOCK_SIZE * DISK_BLOCK_SIZE * DISK_BLOCK_SIZE *
			  DISK_BLOCK_SIZE) {
		pos->axis[0] = 12;
		pos->axis[1] = ptr / (DISK_BLOCK_SIZE * DISK_BLOCK_SIZE *
				      DISK_BLOCK_SIZE);
		ptr %= DISK_BLOCK_SIZE * DISK_BLOCK_SIZE * DISK_BLOCK_SIZE;
		pos->axis[2] = ptr / (DISK_BLOCK_SIZE * DISK_BLOCK_SIZE);
		ptr %= DISK_BLOCK_SIZE * DISK_BLOCK_SIZE;
		pos->axis[3] = ptr / DISK_BLOCK_SIZE;
		return;
	}
}

uint32_t get_inum(uint32_t inode_offset)
{
	inode_t inode;
	read_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	return inode.inum;
}

uint32_t alloc_block()
{
	uint32_t ret = 0;
	for (int i = 0; i < fs.block_map_size; i += DISK_BLOCK_SIZE) {
		char *block_map_buf = read_disk_block(fs.block_map_offset + i);
		for (int j = 0; j < DISK_BLOCK_SIZE; ++j) {
			for (int k = 0; k < 8; ++k) {
				if ((block_map_buf[j] & (1 << k)) == 0) {
					char data = block_map_buf[j] | (1 << k);
					write_disk(fs.block_map_offset + i + j,
						   &data, 1);
					uint32_t block_idx =
						(i * DISK_BLOCK_SIZE + j) * 8 +
						k;
					ret = fs.data_offset +
					      block_idx * DISK_BLOCK_SIZE;
					write_disk(ret, 0, DISK_BLOCK_SIZE);
					return ret;
				}
			}
		}
	}
	return ret;
}

uint32_t get_block(inode_t *inode, uint32_t inode_offset, uint32_t ptr)
{
	axis_t pos;
	parse_inode_ptr(&pos, ptr);

	if (inode->data_ptr[pos.axis[0]] == 0) {
		inode->data_ptr[pos.axis[0]] = alloc_block();
		write_disk(inode_offset, (char *)inode, sizeof(inode_t));
	}

	if (pos.axis[0] < 7) {
		return inode->data_ptr[pos.axis[0]];
	}

	uint32_t first_block_ptr = 0;

	read_disk(inode->data_ptr[pos.axis[0]] + pos.axis[1] * sizeof(uint32_t),
		  (char *)&first_block_ptr, sizeof(uint32_t));

	if (first_block_ptr == 0) {
		first_block_ptr = alloc_block();
		write_disk(inode->data_ptr[pos.axis[0]] +
				   pos.axis[1] * sizeof(uint32_t),
			   (char *)&first_block_ptr, sizeof(uint32_t));
	}

	if (pos.axis[0] < 10) {
		return first_block_ptr;
	}

	uint32_t second_block_ptr = 0;

	read_disk(first_block_ptr + pos.axis[2] * sizeof(uint32_t),
		  (char *)&second_block_ptr, sizeof(uint32_t));

	if (second_block_ptr == 0) {
		second_block_ptr = alloc_block();
		write_disk(first_block_ptr + pos.axis[2] * sizeof(uint32_t),
			   (char *)&second_block_ptr, sizeof(uint32_t));
	}

	if (pos.axis[1] < 12) {
		return second_block_ptr;
	}

	uint32_t third_block_ptr = 0;

	read_disk(third_block_ptr + pos.axis[3] * sizeof(uint32_t),
		  (char *)&third_block_ptr, sizeof(uint32_t));

	if (third_block_ptr == 0) {
		third_block_ptr = alloc_block();
		write_disk(third_block_ptr + pos.axis[3] * sizeof(uint32_t),
			   (char *)&third_block_ptr, sizeof(uint32_t));
	}

	return third_block_ptr;
}

void add_dentry(uint32_t inode_offset, uint32_t inum, char *name)
{
	inode_t inode;
	read_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	uint32_t block_offset = get_block(&inode, inode_offset, inode.size);
	dentry_t dentry;
	dentry.inum = inum;
	strcpy(dentry.name, name);
	write_disk(block_offset + (inode.size % DISK_BLOCK_SIZE),
		   (char *)&dentry, sizeof(dentry_t));
	inode.size += sizeof(dentry_t);
	write_disk(inode_offset, (char *)&inode, sizeof(inode_t));
}

uint32_t get_inode_offset(uint32_t inum)
{
	for (int i = 0; i < fs.inode_size; i += DISK_BLOCK_SIZE) {
		char *inode_buf = read_disk_block(fs.inode_offset + i);
		for (int j = 0; j < DISK_BLOCK_SIZE; j += sizeof(inode_t)) {
			inode_t *ptr = (inode_t *)(inode_buf + j);
			if (ptr->inum == inum) {
				return fs.inode_offset + i + j;
			}
		}
	}
	return 0;
}

uint32_t internel_mkdir(uint32_t fa_inum, char *name)
{
	uint32_t inode_offset = alloc_inode();
	inode_t inode;
	read_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	inode.mode = 1;
	inode.link_num = 2;
	inode.size = 0;
	uint32_t inum = inode.inum;
	write_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	add_dentry(inode_offset, inum, ".");
	if (fa_inum != 0) {
		add_dentry(inode_offset, fa_inum, "..");
		add_dentry(get_inode_offset(fa_inum), inum, name);
	} else {
		add_dentry(inode_offset, inum, "..");
	}
}
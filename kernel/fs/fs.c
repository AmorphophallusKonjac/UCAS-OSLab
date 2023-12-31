#include <os/fs.h>
#include <os/string.h>
#include <os/sched.h>
#include <os/kernel.h>
#include <os/smp.h>
#include <printk.h>
#include <screen.h>

disk_cache_v_idx_t disk_cache_v_idx[DISK_CACHE_NUM];
fd_t fd[FD_NUM];

void print_superblock(fs_t *fs)
{
	printk("[FS] magic : 0x%lX\n", fs->magic);
	printk("[FS] inum ptr : %d\n", fs->inum);
	printk("[FS] size : 0x%lXMB\n", fs->size / 1024 / 1024);
	printk("[FS] block map offset : 0x%lX\n", fs->block_map_offset);
	printk("[FS] inode map offset : 0x%lX\n", fs->inode_map_offset);
	printk("[FS] inode offset : 0x%lX\n", fs->inode_offset);
	printk("[FS] data offset : 0x%lX\n", fs->data_offset);
	printk("[FS] inode size : 0x%lXB, dir entry size : 0x%lXB\n",
	       sizeof(inode_t), sizeof(dentry_t));
}

void init_map(uint32_t offset, uint32_t size)
{
	for (int i = 0; i < size; i += DISK_BLOCK_SIZE) {
		write_disk(offset + i, 0, DISK_BLOCK_SIZE);
	}
}

void init_inode()
{
	internel_mkdir(0, "");
	do_mkdir("/dev");
	do_mkdir("/tmp");
	do_mkdir("/mnt");
}

void do_mkfs()
{
	init_fd();
	local_flush_disk_cache();
	pcb_t *current_running = get_current_running();
	printk("[FS] Start filesystem!\n");
	char *super_block = read_disk_block(0);
	memcpy((uint8_t *)&fs, (uint8_t *)super_block, sizeof(fs_t));
	if (fs.magic == FS_MAGIC && current_running->pid == 0) {
		printk("[FS] Getting superblock...\n");
		print_superblock(&fs);
	} else {
		if (current_running->pid == 0) {
			printk("[FS] Can not find filesystem, create new one!\n");
		} else {
			printk("[FS] Reseting filesystem!\n");
		}
		printk("[FS] Setting superblock...\n");
		fs.magic = FS_MAGIC;
		fs.size = FS_SIZE;
		fs.inum = 0;

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
	inode.mode = FILE;
	inode.link_num = 1;
	inode.size = 0;
	inode.inum = ++fs.inum;
	write_disk(0, (char *)&fs, sizeof(fs_t));
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
					uint32_t inode_idx = (i + j) * 8 + k;
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

	if (ptr < 3ll * DISK_BLOCK_PTR_NUM * DISK_BLOCK_SIZE) {
		pos->axis[0] = ptr / (DISK_BLOCK_PTR_NUM * DISK_BLOCK_SIZE) + 7;
		ptr %= DISK_BLOCK_PTR_NUM * DISK_BLOCK_SIZE;
		pos->axis[1] = ptr / DISK_BLOCK_SIZE;
		pos->axis[2] = 0;
		pos->axis[3] = 0;
		return;
	}

	ptr -= 3 * DISK_BLOCK_PTR_NUM * DISK_BLOCK_SIZE;

	if (ptr <
	    2ll * DISK_BLOCK_PTR_NUM * DISK_BLOCK_PTR_NUM * DISK_BLOCK_SIZE) {
		pos->axis[0] = ptr / (DISK_BLOCK_PTR_NUM * DISK_BLOCK_PTR_NUM *
				      DISK_BLOCK_SIZE) +
			       10;
		ptr %= DISK_BLOCK_PTR_NUM * DISK_BLOCK_PTR_NUM *
		       DISK_BLOCK_SIZE;
		pos->axis[1] = ptr / (DISK_BLOCK_PTR_NUM * DISK_BLOCK_SIZE);
		ptr %= DISK_BLOCK_PTR_NUM * DISK_BLOCK_SIZE;
		pos->axis[2] = ptr / DISK_BLOCK_SIZE;
		pos->axis[3] = 0;
		return;
	}

	ptr -= 2 * DISK_BLOCK_PTR_NUM * DISK_BLOCK_PTR_NUM * DISK_BLOCK_SIZE;

	if (ptr < 1ll * DISK_BLOCK_PTR_NUM * DISK_BLOCK_PTR_NUM *
			  DISK_BLOCK_PTR_NUM * DISK_BLOCK_SIZE) {
		pos->axis[0] = 12;
		pos->axis[1] = ptr / (DISK_BLOCK_PTR_NUM * DISK_BLOCK_PTR_NUM *
				      DISK_BLOCK_SIZE);
		ptr %= DISK_BLOCK_PTR_NUM * DISK_BLOCK_PTR_NUM *
		       DISK_BLOCK_SIZE;
		pos->axis[2] = ptr / (DISK_BLOCK_PTR_NUM * DISK_BLOCK_SIZE);
		ptr %= DISK_BLOCK_PTR_NUM * DISK_BLOCK_SIZE;
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
					uint32_t block_idx = (i + j) * 8 + k;
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

void do_mkdir(char *path)
{
	char *name = NULL;
	char path_bak[BUF_SIZE];
	strcpy(path_bak, path);
	uint32_t dir_inum, fa_dir_inum;
	normalize_path(path);
	parse_path(path, &fa_dir_inum, &dir_inum, DIR, &name);
	if (dir_inum != 0) {
		printk("mkdir: %s File exists\n", path_bak);
		return;
	}
	if (fa_dir_inum == 0) {
		printk("mkdir: Can not find %s\n", path_bak);
		return;
	}
	internel_mkdir(fa_dir_inum, name);
}

uint32_t internel_mkdir(uint32_t fa_inum, char *name)
{
	uint32_t inode_offset = alloc_inode();
	inode_t inode;
	read_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	inode.mode = DIR;
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
	return 0;
}

uint32_t find_file(uint32_t inum, char *name, uint16_t mode)
{
	uint32_t ret = 0;
	if (inum == 0)
		return ret;
	inode_t inode;
	uint32_t inode_offset = get_inode_offset(inum);
	read_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	uint32_t block_offset = 0;
	for (uint32_t ptr = 0; ptr < inode.size; ptr += sizeof(dentry_t)) {
		dentry_t dentry;
		if (ptr % DISK_BLOCK_SIZE == 0) {
			block_offset = get_block(&inode, inode_offset, ptr);
		}
		read_disk(block_offset + (ptr % DISK_BLOCK_SIZE),
			  (char *)&dentry, sizeof(dentry));
		if (strcmp(name, dentry.name) == 0) {
			inode_t file_inode;
			read_disk(get_inode_offset(dentry.inum),
				  (char *)&file_inode, sizeof(inode_t));
			if (file_inode.mode == mode) {
				ret = dentry.inum;
				break;
			}
		}
	}
	return ret;
}

void parse_path(char *path, uint32_t *fa_inum, uint32_t *inum, uint16_t mode,
		char **file_name)
{
	*fa_inum = 1;
	*inum = 1;
	char *name = strtok(path, "/");
	*file_name = name;
	if (name == NULL) {
		if (mode == FILE) {
			*inum = 0;
		}
		return;
	}
	*fa_inum = *inum;
	*inum = find_file(*fa_inum, name, DIR);
	while ((name = strtok(NULL, "/")) != NULL) {
		*file_name = name;
		*fa_inum = *inum;
		*inum = find_file(*fa_inum, name, DIR);
	}
	*inum = find_file(*fa_inum, *file_name, mode);
}

void do_ls(char *path, int detailed)
{
	char path_bak[BUF_SIZE];
	strcpy(path_bak, path);
	normalize_path(path);
	char *name = NULL;
	uint32_t dir_inum, fa_dir_inum;
	parse_path(path, &fa_dir_inum, &dir_inum, DIR, &name);
	if (dir_inum == 0) {
		printk("ls: Can not find %s\n", path_bak);
		return;
	}
	internel_ls(dir_inum, detailed);
}

uint32_t internel_ls(uint32_t dir_inum, int detailed)
{
	inode_t inode;
	uint32_t inode_offset = get_inode_offset(dir_inum);
	read_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	uint32_t block_offset = 0;
	for (uint32_t ptr = 0; ptr < inode.size; ptr += sizeof(dentry_t)) {
		dentry_t dentry;
		if (ptr % DISK_BLOCK_SIZE == 0) {
			block_offset = get_block(&inode, inode_offset, ptr);
		}
		read_disk(block_offset + (ptr % DISK_BLOCK_SIZE),
			  (char *)&dentry, sizeof(dentry));
		if (detailed) {
			inode_t ls_inode;
			read_disk(get_inode_offset(dentry.inum),
				  (char *)&ls_inode, sizeof(inode_t));
			printk("%s   inum : %d   ln_num : %d   size : 0x%X\n",
			       dentry.name, dentry.inum, ls_inode.link_num,
			       ls_inode.size);
		} else {
			printk("   %s", dentry.name);
			if ((ptr / sizeof(dentry_t) % 5 == 4) ||
			    (ptr + sizeof(dentry_t) >= inode.size))
				printk("\n");
		}
	}
	return 0;
}

void do_statfs()
{
	print_superblock(&fs);
	uint32_t used_inode = 0;
	uint32_t used_block = 0;
	for (int i = 0; i < fs.inode_map_size; i += DISK_BLOCK_SIZE) {
		char *inode_map_buf = read_disk_block(fs.inode_map_offset + i);
		for (int j = 0; j < DISK_BLOCK_SIZE; ++j) {
			for (int k = 0; k < 8; ++k) {
				if ((inode_map_buf[j] & (1 << k)) != 0) {
					++used_inode;
				}
			}
		}
	}
	for (int i = 0; i < fs.block_map_size; i += DISK_BLOCK_SIZE) {
		char *block_map_buf = read_disk_block(fs.block_map_offset + i);
		for (int j = 0; j < DISK_BLOCK_SIZE; ++j) {
			for (int k = 0; k < 8; ++k) {
				if ((block_map_buf[j] & (1 << k)) != 0) {
					++used_block;
				}
			}
		}
	}
	printk("inode used : %d/%d\n", used_inode, fs.inode_map_size * 8);
	printk("block used : %d/%d\n", used_block, fs.block_map_size * 8);
}

void normalize_path(char *path)
{
	pcb_t *current_running = get_current_running();
	char new_path[BUF_SIZE];
	if (path[0] == '/') {
		strcpy(new_path, path);
	} else {
		strcpy(new_path, current_running->wd);
		strcat(new_path, "/");
		strcat(new_path, path);
	}
	char path_bak[BUF_SIZE];
	char normalized_path[BUF_SIZE] = "/";
	strcpy(path_bak, new_path);
	char *name = strtok(path_bak, "/");
	if (name == NULL) {
		strcpy(path, normalized_path);
		return;
	}
	if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
		strcat(normalized_path, name);
	}
	while ((name = strtok(NULL, "/")) != NULL) {
		if (strcmp(name, ".") == 0) {
			continue;
		}
		if (strcmp(name, "..") == 0) {
			if (strcmp(normalized_path, "/") == 0) {
				continue;
			}
			int i = strlen(normalized_path) - 1;
			for (; normalized_path[i] != '/'; --i)
				;
			normalized_path[i] = '\0';
		} else {
			strcat(normalized_path, "/");
			strcat(normalized_path, name);
		}
	}
	strcpy(path, normalized_path);
}

void do_cd(char *path)
{
	pcb_t *current_running = get_current_running();
	internel_cd(current_running->wd, path, &current_running->wd_inum);
}

void internel_cd(char *wd, char *path, uint32_t *wd_inum)
{
	char path_bak[BUF_SIZE];
	strcpy(path_bak, path);
	normalize_path(path);
	char new_wd[BUF_SIZE];
	strcpy(new_wd, path);
	uint32_t fa_file_inum, file_inum;
	char *file_name;
	parse_path(path, &fa_file_inum, &file_inum, DIR, &file_name);
	if (file_inum == 0) {
		printk("cd: Can not find %s\n", path_bak);
		return;
	}
	strcpy(wd, new_wd);
	*wd_inum = file_inum;
}

void do_rwd(char *wd)
{
	strcpy(wd, get_current_running()->wd);
}

void do_rmdir(char *path)
{
	pcb_t *current_running = get_current_running();
	char *name = NULL;
	char path_bak[BUF_SIZE];
	strcpy(path_bak, path);
	uint32_t dir_inum, fa_dir_inum;
	normalize_path(path);
	parse_path(path, &fa_dir_inum, &dir_inum, DIR, &name);
	if (dir_inum == 0 || fa_dir_inum == 0) {
		printk("rmdir: Can not find %s\n", path_bak);
		return;
	}
	inode_t dir_inode;
	read_disk(get_inode_offset(dir_inum), (char *)&dir_inode,
		  sizeof(inode_t));
	if (dir_inode.size > 2 * sizeof(dentry_t)) {
		printk("rmdir: failed to remove %s: Directory not empty\n",
		       path_bak);
		return;
	}
	if (dir_inode.inum == current_running->wd_inum) {
		printk("rmdir: failed to remove %s: Danger action\n", path_bak);
		return;
	}
	internel_rmfile(dir_inum, fa_dir_inum, name);
}

void free_block(uint32_t block_offset)
{
	uint32_t block_idx = (block_offset - fs.data_offset) / DISK_BLOCK_SIZE;
	int k = block_idx % 8;
	char data;
	read_disk(fs.block_map_offset + block_idx / 8, &data, 1);
	data = data & (~(1 << k));
	write_disk(fs.block_map_offset + block_idx / 8, &data, 1);
}

void free_inode_block(uint32_t inode_offset)
{
	inode_t inode;
	read_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	axis_t pos;
	parse_inode_ptr(&pos, inode.size);
	uint32_t block_offset[4];

	block_offset[0] = inode.data_ptr[pos.axis[0]];
	read_disk(block_offset[0] + pos.axis[1] * sizeof(uint32_t),
		  (char *)&block_offset[1], sizeof(uint32_t));
	read_disk(block_offset[1] + pos.axis[2] * sizeof(uint32_t),
		  (char *)&block_offset[2], sizeof(uint32_t));
	read_disk(block_offset[2] + pos.axis[3] * sizeof(uint32_t),
		  (char *)&block_offset[3], sizeof(uint32_t));

	if (pos.axis[0] < 7) {
		if (block_offset[0] == 0) {
			return;
		}
		goto direct;
	} else if (pos.axis[0] < 10) {
		if (block_offset[0] == 0 || block_offset[1] == 0) {
			return;
		}
		goto first_level;
	} else if (pos.axis[0] < 12) {
		if (block_offset[0] == 0 || block_offset[1] == 0 ||
		    block_offset[2] == 0) {
			return;
		}
		goto second_level;
	} else {
		if (block_offset[0] == 0 || block_offset[1] == 0 ||
		    block_offset[2] == 0 || block_offset[3] == 0) {
			return;
		}
		goto third_level;
	}

third_level:
	free_block(block_offset[3]);
	write_disk(block_offset[2] + pos.axis[3] * sizeof(uint32_t), 0,
		   sizeof(uint32_t));

	if (pos.axis[3] != 0) {
		return;
	}
second_level:
	free_block(block_offset[2]);
	write_disk(block_offset[1] + pos.axis[2] * sizeof(uint32_t), 0,
		   sizeof(uint32_t));

	if (pos.axis[2] != 0) {
		return;
	}
first_level:
	free_block(block_offset[1]);
	write_disk(block_offset[0] + pos.axis[1] * sizeof(uint32_t), 0,
		   sizeof(uint32_t));

	if (pos.axis[1] != 0) {
		return;
	}
direct:
	free_block(block_offset[0]);
	inode.data_ptr[pos.axis[0]] = 0;
	write_disk(inode_offset, (char *)&inode, sizeof(inode_t));
}

void del_dentry(uint32_t inode_offset, uint32_t inum, char *name)
{
	inode_t inode;
	read_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	uint32_t block_offset = 0;
	for (uint32_t ptr = 0; ptr < inode.size; ptr += sizeof(dentry_t)) {
		dentry_t dentry;
		if (ptr % DISK_BLOCK_SIZE == 0) {
			block_offset = get_block(&inode, inode_offset, ptr);
		}
		read_disk(block_offset + (ptr % DISK_BLOCK_SIZE),
			  (char *)&dentry, sizeof(dentry_t));
		if (dentry.inum == inum && strcmp(dentry.name, name) == 0) {
			dentry_t last_dentry;
			uint32_t last_block_offset =
				get_block(&inode, inode_offset,
					  inode.size - sizeof(dentry_t));
			read_disk(last_block_offset +
					  (inode.size - sizeof(dentry_t)) %
						  DISK_BLOCK_SIZE,
				  (char *)&last_dentry, sizeof(dentry_t));
			write_disk(block_offset + (ptr % DISK_BLOCK_SIZE),
				   (char *)&last_dentry, sizeof(dentry_t));
			break;
		}
	}
	inode.size -= sizeof(dentry_t);
	write_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	if (inode.size % DISK_BLOCK_SIZE == 0) {
		free_inode_block(inode_offset);
	}
}

void free_inode(uint32_t inode_offset)
{
	inode_t inode;
	read_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	for (inode.size = inode.size - (inode.size % DISK_BLOCK_SIZE);;
	     inode.size -= DISK_BLOCK_SIZE) {
		write_disk(inode_offset, (char *)&inode, sizeof(inode_t));
		free_inode_block(inode_offset);
		if (inode.size == 0)
			break;
	}
	uint32_t inode_idx = (inode_offset - fs.inode_offset) / sizeof(inode_t);
	int k = inode_idx % 8;
	char data;
	read_disk(fs.inode_map_offset + inode_idx / 8, &data, 1);
	data = data & (~(1 << k));
	write_disk(fs.inode_map_offset + inode_idx / 8, &data, 1);
}

void internel_rmfile(uint32_t inum, uint32_t fa_dir_inum, char *name)
{
	del_dentry(get_inode_offset(fa_dir_inum), inum, name);
	inode_t inode;
	uint32_t inode_offset = get_inode_offset(inum);
	read_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	if ((inode.mode == FILE && inode.link_num == 1) || inode.mode == DIR) {
		free_inode(inode_offset);
	} else {
		--inode.link_num;
		write_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	}
}

void do_touch(char *path)
{
	char *name = NULL;
	char path_bak[BUF_SIZE];
	strcpy(path_bak, path);
	uint32_t file_inum, fa_dir_inum;
	normalize_path(path);
	parse_path(path, &fa_dir_inum, &file_inum, FILE, &name);
	if (file_inum != 0) {
		printk("touch: %s File exists\n", path_bak);
		return;
	}
	if (fa_dir_inum == 0) {
		printk("touch: Can not find %s\n", path_bak);
		return;
	}
	internel_touch(fa_dir_inum, name);
}

void internel_touch(uint32_t fa_inum, char *name)
{
	uint32_t inode_offset = alloc_inode();
	inode_t inode;
	read_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	inode.mode = FILE;
	inode.link_num = 1;
	inode.size = 0;
	uint32_t inum = inode.inum;
	write_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	add_dentry(get_inode_offset(fa_inum), inum, name);
}

void do_cat(char *path)
{
	char *name = NULL;
	char path_bak[BUF_SIZE];
	strcpy(path_bak, path);
	uint32_t file_inum, fa_dir_inum;
	normalize_path(path);
	parse_path(path, &fa_dir_inum, &file_inum, FILE, &name);
	if (file_inum == 0 || fa_dir_inum == 0) {
		printk("cat: Can not find %s\n", path_bak);
		return;
	}
	internel_cat(file_inum);
}

void internel_cat(uint32_t inum)
{
	inode_t inode;
	uint32_t inode_offset = get_inode_offset(inum);
	read_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	uint32_t block_offset = 0;
	for (uint32_t ptr = 0; ptr < inode.size; ptr++) {
		if (ptr % DISK_BLOCK_SIZE == 0) {
			block_offset = get_block(&inode, inode_offset, ptr);
		}
		char data;
		read_disk(block_offset + (ptr % DISK_BLOCK_SIZE), &data, 1);
		printk("%c", data);
	}
}

void do_ln(char *link_target, char *path)
{
	char *name = NULL;
	char path_bak[BUF_SIZE];
	strcpy(path_bak, path);
	uint32_t file_inum, fa_dir_inum;
	normalize_path(path);
	parse_path(path, &fa_dir_inum, &file_inum, FILE, &name);
	if (file_inum != 0) {
		printk("ln: %s File exists\n", path_bak);
		return;
	}
	if (fa_dir_inum == 0) {
		printk("ln: Can not find %s\n", path_bak);
		return;
	}
	char *target_name = NULL;
	char link_target_bak[BUF_SIZE];
	strcpy(link_target_bak, link_target);
	uint32_t target_file_inum, target_fa_dir_inum;
	normalize_path(link_target);
	parse_path(link_target, &target_fa_dir_inum, &target_file_inum, FILE,
		   &target_name);
	if (target_fa_dir_inum == 0 || target_file_inum == 0) {
		printk("ln: Can not find %s\n", link_target_bak);
		return;
	}
	internel_ln(fa_dir_inum, target_file_inum, name);
}

void internel_ln(uint32_t dir_inum, uint32_t target_inum, char *name)
{
	inode_t inode;
	read_disk(get_inode_offset(target_inum), (char *)&inode,
		  sizeof(inode_t));
	++inode.link_num;
	write_disk(get_inode_offset(target_inum), (char *)&inode,
		   sizeof(inode_t));
	add_dentry(get_inode_offset(dir_inum), target_inum, name);
}

void do_rm(char *path)
{
	char *name = NULL;
	char path_bak[BUF_SIZE];
	strcpy(path_bak, path);
	uint32_t file_inum, fa_dir_inum;
	normalize_path(path);
	parse_path(path, &fa_dir_inum, &file_inum, FILE, &name);
	if (file_inum == 0 || fa_dir_inum == 0) {
		printk("rm: Can not find %s\n", path_bak);
		return;
	}
	internel_rmfile(file_inum, fa_dir_inum, name);
}

void init_fd()
{
	for (int i = 0; i < FD_NUM; ++i) {
		fd[i].access = 0;
		fd[i].inum = 0;
		fd[i].pos = 0;
		fd[i].pid = 0;
	}
}

int do_fopen(char *path, uint32_t access)
{
	char *name = NULL;
	char path_bak[BUF_SIZE];
	strcpy(path_bak, path);
	uint32_t file_inum, fa_dir_inum;
	normalize_path(path);
	parse_path(path, &fa_dir_inum, &file_inum, FILE, &name);
	if (file_inum == 0 || fa_dir_inum == 0) {
		printk("fopen: Can not find %s\n", path_bak);
		return 0;
	}
	return internel_fopen(file_inum, access);
}

int internel_fopen(uint32_t inum, uint32_t access)
{
	int ret = 0;
	for (int i = 0; i < FD_NUM; ++i) {
		spin_lock_acquire(&fd[i].lock);
		if (fd[i].inum == 0) {
			ret = i + 1;
			fd[i].inum = inum;
			fd[i].access = access;
			fd[i].pos = 0;
			fd[i].pid = get_current_running()->pid;
			spin_lock_release(&fd[i].lock);
			return ret;
		}
		spin_lock_release(&fd[i].lock);
	}
	printk("fopen: No more fd\n");
	return ret;
}

void do_fclose(int fd_num)
{
	int idx = fd_num - 1;
	spin_lock_acquire(&fd[idx].lock);
	if (fd[idx].pid == get_current_running()->pid) {
		fd[idx].inum = 0;
		fd[idx].access = 0;
		fd[idx].pos = 0;
		fd[idx].pid = 0;
	} else {
		printk("fclose: you do not own this fd\n");
	}
	spin_lock_release(&fd[idx].lock);
}

int do_fread(int fd_num, char *buff, int size)
{
	int idx = fd_num - 1;
	if (fd[idx].pid != get_current_running()->pid) {
		printk("fread: you do not own this fd\n");
		return 0;
	}
	inode_t inode;
	uint32_t inode_offset = get_inode_offset(fd[idx].inum);
	read_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	if (inode.size - fd[idx].pos < size) {
		size = inode.size - fd[idx].pos;
	}
	uint32_t block_offset = get_block(&inode, inode_offset, fd[idx].pos);
	for (int i = 0; i < size;) {
		if ((fd[idx].pos + i) % DISK_BLOCK_SIZE == 0) {
			block_offset = get_block(&inode, inode_offset,
						 fd[idx].pos + i);
		}
		uint32_t next_i =
			(fd[idx].pos + i) / DISK_BLOCK_SIZE * DISK_BLOCK_SIZE +
			DISK_BLOCK_SIZE - fd[idx].pos;
		next_i = (next_i < size) ? next_i : size;
		read_disk(block_offset + (fd[idx].pos + i) % DISK_BLOCK_SIZE,
			  buff, next_i - i);
		i = next_i;
	}
	fd[idx].pos += size;
	return size;
}

int do_fwrite(int fd_num, char *buff, int size)
{
	int idx = fd_num - 1;
	if (fd[idx].pid != get_current_running()->pid) {
		printk("fwrite: you do not own this fd\n");
		return 0;
	}
	inode_t inode;
	uint32_t inode_offset = get_inode_offset(fd[idx].inum);
	read_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	uint32_t block_offset = get_block(&inode, inode_offset, fd[idx].pos);
	for (int i = 0; i < size;) {
		if ((fd[idx].pos + i) % DISK_BLOCK_SIZE == 0) {
			block_offset = get_block(&inode, inode_offset,
						 fd[idx].pos + i);
		}
		uint32_t next_i =
			(fd[idx].pos + i) / DISK_BLOCK_SIZE * DISK_BLOCK_SIZE +
			DISK_BLOCK_SIZE - fd[idx].pos;
		next_i = (next_i < size) ? next_i : size;
		write_disk(block_offset + (fd[idx].pos + i) % DISK_BLOCK_SIZE,
			   buff, next_i - i);
		i = next_i;
	}
	fd[idx].pos += size;
	inode.size = (fd[idx].pos < inode.size) ? inode.size : fd[idx].pos;
	write_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	return size;
}

int do_lseek(int fd_num, int offset, int whence)
{
	int idx = fd_num - 1;
	if (fd[idx].pid != get_current_running()->pid) {
		printk("lseek: you do not own this fd\n");
		return 0;
	}
	inode_t inode;
	uint32_t inode_offset = get_inode_offset(fd[idx].inum);
	read_disk(inode_offset, (char *)&inode, sizeof(inode_t));
	int pos = 0;
	switch (whence) {
	case SEEK_SET:
		pos = offset;
		if (pos < 0 || pos > inode.size) {
			printk("lseek: error offset\n");
			return 1;
		}
		break;
	case SEEK_CUR:
		pos += offset;
		if (pos < 0 || pos > inode.size) {
			printk("lseek: error offset\n");
			return 1;
		}
		break;
	case SEEK_END:
		pos = inode.size + offset;
		inode.size += offset;
		write_disk(inode_offset, (char *)&inode, sizeof(inode_t));
		break;
	default:
		printk("lseek: unknown whence\n");
		return 1;
	}
	fd[idx].pos = pos;
	return 0;
}

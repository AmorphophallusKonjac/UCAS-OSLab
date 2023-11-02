# -----------------------------------------------------------------------
# Project Information
# -----------------------------------------------------------------------

PROJECT_IDX	= 3

# -----------------------------------------------------------------------
# Host Linux Variables
# -----------------------------------------------------------------------

SHELL       = /bin/sh
DISK        = /dev/sdb
TTYUSB1     = /dev/ttyUSB1
DIR_OSLAB   = $(HOME)/OSLab-RISC-V
DIR_QEMU    = $(DIR_OSLAB)/qemu
DIR_UBOOT   = $(DIR_OSLAB)/u-boot

# -----------------------------------------------------------------------
# Build and Debug Tools
# -----------------------------------------------------------------------

HOST_CC         = gcc
CROSS_PREFIX    = riscv64-unknown-linux-gnu-
CC              = $(CROSS_PREFIX)gcc
AR              = $(CROSS_PREFIX)ar
OBJDUMP         = $(CROSS_PREFIX)objdump
GDB             = $(CROSS_PREFIX)gdb
QEMU            = $(DIR_QEMU)/riscv64-softmmu/qemu-system-riscv64
UBOOT           = $(DIR_UBOOT)/u-boot
MINICOM         = minicom

# -----------------------------------------------------------------------
# Build/Debug Flags and Variables
# -----------------------------------------------------------------------

CFLAGS          = -O0 -fno-builtin -nostdlib -nostdinc -Wall -mcmodel=medany -ggdb3

BOOT_INCLUDE    = -I$(DIR_ARCH)/include
BOOT_CFLAGS     = $(CFLAGS) $(BOOT_INCLUDE) -Wl,--defsym=TEXT_START=$(BOOTLOADER_ENTRYPOINT) -T riscv.lds

KERNEL_INCLUDE  = -I$(DIR_ARCH)/include -Iinclude -Idrivers
KERNEL_CFLAGS   = $(CFLAGS) $(KERNEL_INCLUDE) -Wl,--defsym=TEXT_START=$(KERNEL_ENTRYPOINT) -T riscv.lds

USER_INCLUDE    = -I$(DIR_TINYLIBC)/include
USER_CFLAGS     = $(CFLAGS) $(USER_INCLUDE)
USER_LDFLAGS    = -L$(DIR_BUILD) -ltinyc

QEMU_LOG_FILE   = $(DIR_OSLAB)/oslab-log.txt
QEMU_OPTS       = -nographic -machine virt -m 256M -kernel $(UBOOT) -bios none \
                     -drive if=none,format=raw,id=image,file=${ELF_IMAGE} \
                     -device virtio-blk-device,drive=image \
                     -monitor telnet::45454,server,nowait -serial mon:stdio \
                     -D $(QEMU_LOG_FILE) -d oslab
QEMU_DEBUG_OPT  = -s -S
QEMU_SMP_OPT	= -smp 2

# -----------------------------------------------------------------------
# UCAS-OS Entrypoints and Variables
# -----------------------------------------------------------------------

DIR_ARCH        = ./arch/riscv
DIR_BUILD       = ./build
DIR_DRIVERS     = ./drivers
DIR_INIT        = ./init
DIR_KERNEL      = ./kernel
DIR_LIBS        = ./libs
DIR_TINYLIBC    = ./tiny_libc
DIR_TEST        = ./test
DIR_TEST_PROJ   = $(DIR_TEST)/test_project$(PROJECT_IDX)

BOOTLOADER_ENTRYPOINT   = 0x50200000
KERNEL_ENTRYPOINT       = 0x50201000
USER_ENTRYPOINT         = 0x52000000

# -----------------------------------------------------------------------
# UCAS-OS Kernel Source Files
# -----------------------------------------------------------------------

SRC_BOOT    = $(wildcard $(DIR_ARCH)/boot/*.S)
SRC_ARCH    = $(wildcard $(DIR_ARCH)/kernel/*.S)
SRC_BIOS    = $(wildcard $(DIR_ARCH)/bios/*.c)
SRC_DRIVER  = $(wildcard $(DIR_DRIVERS)/*.c)
SRC_INIT    = $(wildcard $(DIR_INIT)/*.c)
SRC_KERNEL  = $(wildcard $(DIR_KERNEL)/*/*.c)
SRC_LIBS    = $(wildcard $(DIR_LIBS)/*.c)

SRC_MAIN    = $(SRC_ARCH) $(SRC_INIT) $(SRC_BIOS) $(SRC_DRIVER) $(SRC_KERNEL) $(SRC_LIBS)

ELF_BOOT    = $(DIR_BUILD)/bootblock
ELF_MAIN    = $(DIR_BUILD)/main
ELF_IMAGE   = $(DIR_BUILD)/image

# -----------------------------------------------------------------------
# UCAS-OS User Source Files
# -----------------------------------------------------------------------

SRC_CRT0    = $(wildcard $(DIR_ARCH)/crt0/*.S)
DEFLATE_SRC_CRT0 = $(wildcard $(DIR_ARCH)/decompress-crt0/*.S)
OBJ_CRT0    = $(DIR_BUILD)/$(notdir $(SRC_CRT0:.S=.o))

SRC_LIBC    = $(wildcard ./tiny_libc/*.c)
OBJ_LIBC    = $(patsubst %.c, %.o, $(foreach file, $(SRC_LIBC), $(DIR_BUILD)/$(notdir $(file))))
LIB_TINYC   = $(DIR_BUILD)/libtinyc.a

SRC_SHELL	= $(DIR_TEST)/shell.c
SRC_USER    = $(SRC_SHELL) $(wildcard $(DIR_TEST_PROJ)/*.c)
ELF_USER    = $(patsubst %.c, %, $(foreach file, $(SRC_USER), $(DIR_BUILD)/$(notdir $(file))))

# -----------------------------------------------------------------------
# Host Linux Tools Source Files
# -----------------------------------------------------------------------

SRC_CREATEIMAGE = ./tools/createimage.c
ELF_CREATEIMAGE = $(DIR_BUILD)/$(notdir $(SRC_CREATEIMAGE:.c=))

DEFLATE_DIR		= ./tools/deflate
DEFLATE_LIB_DIR = $(DEFLATE_DIR)/lib

DEFLATE_INCLUDE_COMMON = -I$(DEFLATE_LIB_DIR) -I$(DEFLATE_DIR)
DEFLATE_CFLAGS_COMMON  = -O2 $(DEFLATE_INCLUDE_COMMON) -Wall -DFREESTANDING

DEFLATE_CFLAGS_DEPRESS = $(CFLAGS) -DFREESTANDING $(DEFLATE_INCLUDE_COMMON) -I./arch/riscv/include

SRC_COMPRESS 		= ./tools/compress.c
SRC_DECOMPRESS  	= ./tools/decompress.c
DEFLATE_SRC_LIB     = $(DEFLATE_LIB_DIR)/*.c
DEFLATE_SRC_COMMON  = $(DEFLATE_DIR)/*.c
ELF_COMPRESS	= $(DIR_BUILD)/compress
ELF_DECOMPRESS	= $(DIR_BUILD)/decompress

# -----------------------------------------------------------------------
# Top-level Rules
# -----------------------------------------------------------------------

all: dirs elf image asm # floppy

dirs:
	@mkdir -p $(DIR_BUILD)

format:
	find . -regex '.*\.\(cpp\|hpp\|cu\|c\|h\)' -exec clang-format-11 -style=file -i {} \;

clean:
	rm -rf $(DIR_BUILD)

floppy:
	sudo fdisk -l $(DISK)
	sudo dd if=$(DIR_BUILD)/image of=$(DISK)3 conv=notrunc

asm: $(ELF_BOOT) $(ELF_MAIN) $(ELF_USER) $(ELF_DECOMPRESS)
	for elffile in $^; do $(OBJDUMP) -d $$elffile > $(notdir $$elffile).txt; done

gdb:
	$(GDB) $(ELF_MAIN) -ex "target remote:1234"

run:
	$(QEMU) $(QEMU_OPTS)

run-smp:
	$(QEMU) $(QEMU_OPTS) $(QEMU_SMP_OPT)

debug:
	$(QEMU) $(QEMU_OPTS) $(QEMU_DEBUG_OPT)

debug-smp:
	$(QEMU) $(QEMU_OPTS) $(QEMU_SMP_OPT) $(QEMU_DEBUG_OPT)

minicom:
	sudo $(MINICOM) -D $(TTYUSB1)

.PHONY: all dirs clean floppy asm gdb run debug viewlog minicom

# -----------------------------------------------------------------------
# UCAS-OS Rules
# -----------------------------------------------------------------------

$(ELF_BOOT): $(SRC_BOOT) riscv.lds
	$(CC) $(BOOT_CFLAGS) -o $@ $(SRC_BOOT) -e main

$(ELF_MAIN): $(SRC_MAIN) riscv.lds
	$(CC) $(KERNEL_CFLAGS) -o $@ $(SRC_MAIN)

$(ELF_DECOMPRESS): $(SRC_DECOMPRESS) $(DEFLATE_SRC_LIB) $(DEFLATE_SRC_COMMON) $(SRC_CRT0)
	$(CC) $(SRC_DECOMPRESS) $(SRC_CRT0) $(DEFLATE_SRC_LIB) $(DEFLATE_SRC_COMMON) $(DEFLATE_CFLAGS_DEPRESS) -g -o $@ -Wl,--defsym=TEXT_START=0x51000000 -T riscv.lds

$(OBJ_CRT0): $(SRC_CRT0)
	$(CC) $(USER_CFLAGS) -I$(DIR_ARCH)/include -c $< -o $@

$(LIB_TINYC): $(OBJ_LIBC)
	$(AR) rcs $@ $^

$(DIR_BUILD)/%.o: $(DIR_TINYLIBC)/%.c
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(DIR_BUILD)/%: $(DIR_TEST_PROJ)/%.c $(OBJ_CRT0) $(LIB_TINYC) riscv.lds
	$(CC) $(USER_CFLAGS) -o $@ $(OBJ_CRT0) $< $(USER_LDFLAGS) -Wl,--defsym=TEXT_START=$(USER_ENTRYPOINT) -T riscv.lds
	$(eval USER_ENTRYPOINT := $(shell python3 -c "print(hex(int('$(USER_ENTRYPOINT)', 16) + int('0x10000', 16)))"))

$(DIR_BUILD)/%: $(DIR_TEST)/%.c $(OBJ_CRT0) $(LIB_TINYC) riscv.lds
	$(CC) $(USER_CFLAGS) -o $@ $(OBJ_CRT0) $< $(USER_LDFLAGS) -Wl,--defsym=TEXT_START=$(USER_ENTRYPOINT) -T riscv.lds
	$(eval USER_ENTRYPOINT := $(shell python3 -c "print(hex(int('$(USER_ENTRYPOINT)', 16) + int('0x10000', 16)))"))

elf: $(ELF_BOOT) $(ELF_MAIN) $(LIB_TINYC) $(ELF_USER) $(ELF_DECOMPRESS)

.PHONY: elf

# -----------------------------------------------------------------------
# Host Linux Rules
# -----------------------------------------------------------------------

$(ELF_COMPRESS): $(SRC_COMPRESS) $(DEFLATE_SRC_LIB) $(DEFLATE_SRC_COMMON)
	$(HOST_CC) $(SRC_COMPRESS) $(DEFLATE_SRC_LIB) $(DEFLATE_SRC_COMMON) $(DEFLATE_CFLAGS_COMMON) -g -o $@ 

$(ELF_CREATEIMAGE): $(SRC_CREATEIMAGE) $(DEFLATE_SRC_LIB) $(DEFLATE_SRC_COMMON)
	$(HOST_CC) $(SRC_CREATEIMAGE) $(DEFLATE_SRC_LIB) $(DEFLATE_SRC_COMMON) $(DEFLATE_CFLAGS_COMMON) -o $@ -ggdb -Wall 

image: $(ELF_CREATEIMAGE) $(ELF_BOOT) $(ELF_DECOMPRESS) $(ELF_MAIN) $(ELF_USER) 
	cd $(DIR_BUILD) &&./$(<F) --extended $(filter-out $(<F) compress, $(^F))

.PHONY: image

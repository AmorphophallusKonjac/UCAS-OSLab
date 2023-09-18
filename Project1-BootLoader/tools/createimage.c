#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "deflate/tinylibdeflate.h"

#define IMAGE_FILE "./image"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

#define SECTOR_SIZE 512
#define BOOT_LOADER_SIG_OFFSET 0x1fe
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
#define BOOT_LOADER_SIG_1 0x55
#define BOOT_LOADER_SIG_2 0xaa

#define NBYTES2SEC(nbytes) \
    (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

/* TODO: [p1-task4] design your own task_info_t */
typedef struct {
    uint32_t offset;
    uint32_t size;
    char name[32];
    uint64_t entrypoint;
} task_info_t;

#define TASK_MAXNUM 16
static task_info_t taskinfo[TASK_MAXNUM];

/* structure to store command line options */
static struct {
    int vm;
    int extended;
} options;

/* prototypes of local functions */
static void create_image(int nfiles, char *files[]);
static void error(char *fmt, ...);
static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp);
static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr);
static uint64_t get_entrypoint(Elf64_Ehdr ehdr);
static uint32_t get_filesz(Elf64_Phdr phdr);
static uint32_t get_memsz(Elf64_Phdr phdr);
static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr);
static void write_padding(FILE *img, int *phyaddr, int new_phyaddr);
static void write_img_info(int nbytes_kernel, int nbytes_decompress,
                           task_info_t *taskinfo, short tasknum, FILE *img);
static void get_compress_data(Elf64_Phdr phdr, char *data, int *data_len, FILE *fp);

int main(int argc, char **argv) {
    char *progname = argv[0];

    /* process command line options */
    options.vm = 0;
    options.extended = 0;
    while ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == '-')) {
        char *option = &argv[1][2];

        if (strcmp(option, "vm") == 0) {
            options.vm = 1;
        } else if (strcmp(option, "extended") == 0) {
            options.extended = 1;
        } else {
            error("%s: invalid option\nusage: %s %s\n", progname, progname, ARGS);
        }
        argc--;
        argv++;
    }
    if (options.vm == 1) {
        error("%s: option --vm not implemented\n", progname);
    }
    if (argc < 3) {
        /* at least 3 args (createimage bootblock main) */
        error("usage: %s %s\n", progname, ARGS);
    }
    create_image(argc - 1, argv + 1);
    return 0;
}

/* TODO: [p1-task4] assign your task_info_t somewhere in 'create_image' */
static void create_image(int nfiles, char *files[]) {
    int tasknum = nfiles - 3;
    int nbytes_kernel = 0;
    int phyaddr = 0;
    int nbytes_decompress = 0;
    FILE *fp = NULL, *img = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Phdr phdr;

    /* open the image file */
    img = fopen(IMAGE_FILE, "w");
    assert(img != NULL);

    /* for each input file */
    for (int fidx = 0; fidx < nfiles; ++fidx) {
        int taskidx = fidx - 3;

        /* open input file */
        fp = fopen(*files, "r");
        assert(fp != NULL);

        /* read ELF header */
        read_ehdr(&ehdr, fp);
        printf("0x%04lx: %s\n", ehdr.e_entry, *files);

        if (strcmp(*files, "main") == 0) {
            fseek(fp, 0, SEEK_END);
            nbytes_kernel = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            char *data = (char *)malloc(nbytes_kernel);
            int data_len = 0;
            memset(data, 0, nbytes_kernel);

            /* for each program header */
            for (int ph = 0; ph < ehdr.e_phnum; ph++) {
                /* read program header */
                read_phdr(&phdr, fp, ph, ehdr);

                if (phdr.p_type != PT_LOAD)
                    continue;

                /* get segment to the data */
                get_compress_data(phdr, data, &data_len, fp);
            }

            /* prepare environment */
            deflate_set_memory_allocator((void *(*)(int))malloc, free);
            struct libdeflate_compressor *compressor = deflate_alloc_compressor(12);
            char *compressed = (char *)malloc(data_len + 10);
            memset(compressed, 0, data_len + 10);
            /* do compress */
            int out_nbytes = deflate_deflate_compress(compressor, data, data_len,
                                                      compressed, data_len + 10);
            /* write back to main*/
            size_t bytes_write = fwrite(compressed, 1, out_nbytes, img);
            if (bytes_write != out_nbytes) {
                printf("main write fail!");
            }
            phyaddr += bytes_write;
            // printf("compressed kernel ");
            fclose(fp);
            files++;
            continue;
        }

        // [p1-task4] create app info:offset, name, entrypoint
        if (taskidx >= 0) {
            taskinfo[taskidx].offset = phyaddr;
            strcpy(taskinfo[taskidx].name, *files);
            taskinfo[taskidx].entrypoint = get_entrypoint(ehdr);
        }

        /* for each program header */
        for (int ph = 0; ph < ehdr.e_phnum; ph++) {
            /* read program header */
            read_phdr(&phdr, fp, ph, ehdr);

            if (phdr.p_type != PT_LOAD)
                continue;

            /* write segment to the image */
            write_segment(phdr, fp, img, &phyaddr);

            /* update nbytes_decompress */
            if (strcmp(*files, "decompress") == 0) {
                nbytes_decompress += get_filesz(phdr);
            }
        }

        // [p1-task4] create app info:size
        if (taskidx >= 0) {
            taskinfo[taskidx].size = phyaddr - taskinfo[taskidx].offset;
        }

        /* write padding bytes */
        /**
     * TODO:
     * 1. [p1-task3] do padding so that the kernel and every app program
     *  occupies the same number of sectors
     * 2. [p1-task4] only padding bootblock is allowed!
     */
        if (strcmp(*files, "bootblock") == 0) {
            write_padding(img, &phyaddr, SECTOR_SIZE);
        }
        // [p1-task3] padding kernel and app
        // else {
        //     int program_size = 15 * SECTOR_SIZE;
        //     int new_phyaddr =  (
        //         (phyaddr - SECTOR_SIZE) / program_size
        //         + ((phyaddr - SECTOR_SIZE) % program_size != 0)
        //     ) * program_size + SECTOR_SIZE;
        //     write_padding(img, &phyaddr, new_phyaddr);
        // }

        fclose(fp);
        files++;
    }
    write_img_info(nbytes_kernel, nbytes_decompress, taskinfo, tasknum, img);

    fclose(img);
}

static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp) {
    int ret;

    ret = fread(ehdr, sizeof(*ehdr), 1, fp);
    assert(ret == 1);
    assert(ehdr->e_ident[EI_MAG1] == 'E');
    assert(ehdr->e_ident[EI_MAG2] == 'L');
    assert(ehdr->e_ident[EI_MAG3] == 'F');
}

static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr) {
    int ret;

    fseek(fp, ehdr.e_phoff + ph * ehdr.e_phentsize, SEEK_SET);
    ret = fread(phdr, sizeof(*phdr), 1, fp);
    assert(ret == 1);
    if (options.extended == 1) {
        printf("\tsegment %d\n", ph);
        printf("\t\toffset 0x%04lx", phdr->p_offset);
        printf("\t\tvaddr 0x%04lx\n", phdr->p_vaddr);
        printf("\t\tfilesz 0x%04lx", phdr->p_filesz);
        printf("\t\tmemsz 0x%04lx\n", phdr->p_memsz);
    }
}

static uint64_t get_entrypoint(Elf64_Ehdr ehdr) {
    return ehdr.e_entry;
}

static uint32_t get_filesz(Elf64_Phdr phdr) {
    return phdr.p_filesz;
}

static uint32_t get_memsz(Elf64_Phdr phdr) {
    return phdr.p_memsz;
}

static void get_compress_data(Elf64_Phdr phdr, char *data, int *data_len, FILE *fp) {
    if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD) {
        if (options.extended == 1) {
            printf("\t\twriting 0x%04lx bytes\n", phdr.p_filesz);
        }
        fseek(fp, phdr.p_offset, SEEK_SET);
        while (phdr.p_filesz-- > 0) {
            data[*data_len] = fgetc(fp);
            (*data_len)++;
        }
    }
}

static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr) {
    if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD) {
        /* write the segment itself */
        /* NOTE: expansion of .bss should be done by kernel or runtime env! */
        if (options.extended == 1) {
            printf("\t\twriting 0x%04lx bytes\n", phdr.p_filesz);
        }
        fseek(fp, phdr.p_offset, SEEK_SET);
        while (phdr.p_filesz-- > 0) {
            fputc(fgetc(fp), img);
            (*phyaddr)++;
        }
    }
}

static void write_padding(FILE *img, int *phyaddr, int new_phyaddr) {
    if (options.extended == 1 && *phyaddr < new_phyaddr) {
        printf("\t\twrite 0x%04x bytes for padding\n", new_phyaddr - *phyaddr);
    }

    while (*phyaddr < new_phyaddr) {
        fputc(0, img);
        (*phyaddr)++;
    }
}

static void write_img_info(int nbytes_kernel, int nbytes_decompress,
                           task_info_t *taskinfo, short tasknum, FILE *img) {
    // TODO: [p1-task3] & [p1-task4] write image info to some certain places
    // NOTE: os size, infomation about app-info sector(s) ...
    // [p1-task4] write app info
    fwrite(taskinfo, sizeof(task_info_t), tasknum, img);

    // [p1-task5]: calc decompress sector num and task num, save it in OS_SIZE_LOC
    uint16_t decompress_sector_num = NBYTES2SEC(nbytes_decompress);
    fseek(img, OS_SIZE_LOC, SEEK_SET);
    fwrite(&decompress_sector_num, sizeof(uint16_t), 1, img);
    fwrite(&tasknum, sizeof(short), 1, img);

    // [p1-task4]: calc taskinfo sector num and sector id and taskinfo offset and
    // taskinfo size
    uint32_t taskinfo_offset =
        taskinfo[tasknum - 1].offset + taskinfo[tasknum - 1].size;
    uint32_t taskinfo_size = sizeof(task_info_t) * tasknum;
    uint16_t taskinfo_sector_id = taskinfo_offset / SECTOR_SIZE;
    uint16_t taskinfo_sector_num =
        (taskinfo_offset + taskinfo_size) / SECTOR_SIZE - taskinfo_sector_id + 1;
    fseek(img, OS_SIZE_LOC - 12, SEEK_SET);
    fwrite(&taskinfo_offset, sizeof(uint32_t), 1, img);
    fwrite(&taskinfo_size, sizeof(uint32_t), 1, img);
    fwrite(&taskinfo_sector_id, sizeof(uint16_t), 1, img);
    fwrite(&taskinfo_sector_num, sizeof(uint16_t), 1, img);

    // [p1-task5]: save nbytes_decompress, nbytes_kernel
    fseek(img, OS_SIZE_LOC - 12 - 8, SEEK_SET);
    fwrite(&nbytes_decompress, sizeof(int), 1, img);
    fwrite(&nbytes_kernel, sizeof(int), 1, img);

    // [p1-task5]: calc kernel_offset and kernel_sector_id and kernel_sector_num
    uint32_t kernel_offset = SECTOR_SIZE + nbytes_decompress;
    uint16_t kernel_sector_id = kernel_offset / SECTOR_SIZE;
    uint16_t kernel_sector_num =
        (kernel_offset + nbytes_kernel) / SECTOR_SIZE - kernel_sector_id + 1;
    fseek(img, OS_SIZE_LOC - 12 - 8 - 8, SEEK_SET);
    fwrite(&kernel_offset, sizeof(uint32_t), 1, img);
    fwrite(&kernel_sector_id, sizeof(uint16_t), 1, img);
    fwrite(&kernel_sector_num, sizeof(uint16_t), 1, img);
}

/* print an error message and exit */
static void error(char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if (errno != 0) {
        perror(NULL);
    }
    exit(EXIT_FAILURE);
}

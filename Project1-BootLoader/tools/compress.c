/*
main2kernel:generate main in image
kernel2main:compress main
*/

#include "deflate/tinylibdeflate.h"
#include <elf.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


#define MAIN "./main"
#define KERNEL "./kernel"

static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp);
static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr);
static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr);
static uint32_t get_filesz(Elf64_Phdr phdr);

int main() {
    int nbytes_kernel = 0;
    int phyaddr = 0;
    FILE *fp = NULL, *cfp = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Phdr phdr;

    // ==== main2kernel ====
    /* open the main */
    cfp = fopen(MAIN, "r");
    assert(cfp != NULL);

    /* open the kernel */
    fp = fopen(KERNEL, "w");
    assert(fp != NULL);

    read_ehdr(&ehdr, cfp);

    /* for each program header */
    for (int ph = 0; ph < ehdr.e_phnum; ph++) {
        
        /* read program header */
        read_phdr(&phdr, cfp, ph, ehdr);

        if (phdr.p_type != PT_LOAD) continue;

        /* write segment to the image */
        write_segment(phdr, cfp, fp, &phyaddr);

        /* update nbytes_kernel */
        nbytes_kernel += get_filesz(phdr);
    }
    fclose(fp);
    fclose(cfp);

    // ==== kernel2main ====
    /* open the main */
    cfp = fopen(MAIN, "wb");
    assert(cfp != NULL);

    /* open the kernel */
    fp = fopen(KERNEL, "rb");
    assert(fp != NULL);

    /* read kernel to data */
    char *data = (char *)malloc(nbytes_kernel);
    memset(data, 0, nbytes_kernel);
    size_t bytes_read = fread(data, nbytes_kernel, 1, fp);
    if (bytes_read != nbytes_kernel) {
        printf("read fail!!!!!\n");
    }
    fclose(fp);

    /* prepare environment */ 
    deflate_set_memory_allocator((void * (*)(int))malloc, free);
    struct libdeflate_compressor * compressor = deflate_alloc_compressor(1);
    char *compressed = (char *)malloc(nbytes_kernel + 10);
    memset(compressed, 0, nbytes_kernel + 10);

    /* do compress */ 
    int out_nbytes = deflate_deflate_compress(compressor, data, nbytes_kernel, compressed, nbytes_kernel + 10);
    
    /* write back to main*/
    size_t bytes_write = fwrite(compressed, out_nbytes, 1, cfp);
    if (bytes_write != out_nbytes) {
        printf("write fail!!!");
    }
    free(data);
    free(compressed);
    return 0;
}

static void read_ehdr(Elf64_Ehdr * ehdr, FILE * fp)
{
    int ret;

    ret = fread(ehdr, sizeof(*ehdr), 1, fp);
    assert(ret == 1);
    assert(ehdr->e_ident[EI_MAG1] == 'E');
    assert(ehdr->e_ident[EI_MAG2] == 'L');
    assert(ehdr->e_ident[EI_MAG3] == 'F');
}

static void read_phdr(Elf64_Phdr * phdr, FILE * fp, int ph,
                      Elf64_Ehdr ehdr)
{
    int ret;

    fseek(fp, ehdr.e_phoff + ph * ehdr.e_phentsize, SEEK_SET);
    ret = fread(phdr, sizeof(*phdr), 1, fp);
    assert(ret == 1);
}

static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr)
{
    if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD) {
        /* write the segment itself */
        /* NOTE: expansion of .bss should be done by kernel or runtime env! */
        fseek(fp, phdr.p_offset, SEEK_SET);
        while (phdr.p_filesz-- > 0) {
            fputc(fgetc(fp), img);
            (*phyaddr)++;
        }
    }
}

static uint32_t get_filesz(Elf64_Phdr phdr)
{
    return phdr.p_filesz;
}
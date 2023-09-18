#include "deflate/tinylibdeflate.h"

extern struct libdeflate_decompressor global_decompressor;

#define DECOM_KERNEL_LOC 0x54000000
#define KERNEL 0x50201000
#define OS_SIZE_LOC 0x502001fc
#define BUF_LEN 0x51ffffff - 0x50200000
#define SECTOR_SIZE 512

int main() {
    int nbytes_compress = *(int *)(OS_SIZE_LOC - 20);
    int nbytes_kernel = *(int *)(OS_SIZE_LOC - 16);
    int kernel_offset = SECTOR_SIZE + nbytes_compress;
    kernel_offset = kernel_offset % SECTOR_SIZE;
    // prepare environment
    struct libdeflate_decompressor *decompressor = deflate_alloc_decompressor();

    char *compressed = (char *)(DECOM_KERNEL_LOC + kernel_offset);
    char *extracted = (char *)(KERNEL);

    // do decompress
    int restore_nbytes = 0;
    deflate_deflate_decompress(&global_decompressor, compressed, nbytes_kernel, extracted, BUF_LEN, &restore_nbytes);

    return 0;
}
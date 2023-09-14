#include "deflate/tinylibdeflate.h"


#define DECOM_KERNEL_LOC 0x52000000
#define KERNEL 0x50201000
#define OS_SIZE_LOC 0x502001fc
#define BUF_LEN 0x51ffffff - 0x50200000


int main(){
    // prepare environment
    struct libdeflate_decompressor * decompressor = deflate_alloc_decompressor();

    int nbytes_compress = *(int *)(OS_SIZE_LOC - 20);
    int nbytes_kernel = *(int *)(OS_SIZE_LOC - 16);
    char *compressed = (char *)(DECOM_KERNEL_LOC + nbytes_compress);
    char *extracted = (char *)(KERNEL);

    // do decompress
    int restore_nbytes = 0;
    deflate_deflate_decompress(decompressor, compressed, nbytes_kernel, extracted, BUF_LEN, &restore_nbytes);

    return 0;
}
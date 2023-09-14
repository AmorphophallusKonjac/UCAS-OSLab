#include "deflate/tinylibdeflate.h"


#define COM_KERNEL_LOC 0x52000000
#define KERNEL 0x50201000
#define OS_SIZE_LOC 0x502001fc
#define BUF_LEN 0x51ffffff - 0x50200000


int main(){
    // prepare environment
    struct libdeflate_decompressor * decompressor = deflate_alloc_decompressor();

    char *compressed = (char *)(COM_KERNEL_LOC);
    char *extracted = (char *)(KERNEL);
    int nbytes_kernel = *(int *)(OS_SIZE_LOC - 0x10);

    // do decompress
    int restore_nbytes = 0;
    deflate_deflate_decompress(decompressor, compressed, nbytes_kernel, extracted, BUF_LEN, &restore_nbytes);

    return 0;
}
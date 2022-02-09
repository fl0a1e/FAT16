#include "io.h"

#include <stdio.h>

FILE *image;


int init_myio(const char* filename) {
    if(!(image = fopen(filename, "r+b"))){
        return -1;
    }

    return 0;
}

size_t io_read(void *buf, long offset, size_t size){
    if(0 != fseek(image, offset, SEEK_SET)){
        return 0;
    }
    
    return fread(buf, 1, size, image);
}

size_t io_write(void *buf, long offset, size_t size){
    if(0 != fseek(image, offset, SEEK_SET)){
        return 0;
    }
    
    size_t ret = fwrite(buf, 1, size, image);
    fflush(image);

    return ret;
}

void io_release() {
    if (image) {
        fclose(image);
    }
}
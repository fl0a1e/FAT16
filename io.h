#ifndef IO_H
#define IO_H

#include <stddef.h>

// open image file
// 0:sucess 负数:fail
int init_myio(const char* filename);


// 保存数据缓冲，读取起点，读取长度
// 返回读取的数据
size_t io_read(void *buf, long offset, size_t size);

/**
 * write to image
 * 数据缓冲，起点，写入长度
 * @return 返回写入长度
 */ 
size_t io_write(void *buf, long offset, size_t size);

/**
 * release all resource
 */
void io_release();

#endif
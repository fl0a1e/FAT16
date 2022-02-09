#ifndef UTILS_H
#define UTILS_H

#include "fat16.h"
#include "io.h"

// 
struct FindOption {
    // input
    const char *name;

    // output
    long pos;           // 目标簇在image的偏移量
    int index;          // 目标fcb在dir（簇）中的偏移
    struct FCB fcb;     // 目标fcb
};

// readdir_callback 函数的 opt 参数
struct ReadDirOption {
	// 输入
	fuse_fill_dir_t filler;
	void *buf;
};


// 查找文件的FCB
// 路径，填充FCB的结构体
// 返回FCB在image的偏移
long find_fcb(const char *path, struct FCB *ret);

// 文件控制块
// filename返回文件名
// 返回文件名第一个字节，0表示目录项截至，0xe5表示文件目录项被删除
void get_filename(const struct FCB *fcb, char *filename);

// 根据fat表找下一个簇的位置
// 返回簇号
uint16_t next_cluster(uint16_t cluster);

// 根据簇号返回偏移量
// 返回偏移量，-1表示簇不存在
long get_cluster_offset(uint16_t cluster);


// 遍历根目录
// 对找到的fcb调用回调函数
int traverse_root_dir(void *opt,
	int (*callback)(void *data, long pos, int index, const struct FCB *fcb));

// 遍历子目录
int traverse_sub_dir(const struct FCB *fcb, void *opt,
	int (*callback)(void *data, long pos, int index, const struct FCB *fcb));

// 查找文件
int find_file_callback(void *opt, long pos, int index, const struct FCB *fcb);

// 读目录
int readdir_callback(void *opt, long pos, int index, const struct FCB *fcb);

// 添加目录表项
int get_free_entry_callback(void *opt, long pos, int index, const struct FCB *fcb);

// 读文件
int read_file(const struct FCB *fcb, void *buff, off_t offset, size_t size);

// 写文件
int write_file(struct FCB *fcb, long fcb_offset, void *buff, off_t offset, size_t size);

// 释放文件占有的簇
// 输入文件起始簇号
void release_cluster(uint16_t cluster);

// 释放文件
// 成功返回1
int remove_file(struct FCB *file, long offset_fcb);

// 返回1表示在使用
int is_cluster_inuse(uint16_t cluster_num);

// 获取文件占有的簇数量
uint32_t get_cluster_count(struct FCB *fcb);

// 判断文件名是否合法
int is_filename_available(const char *filename);

// 扩容
// 返回第一个簇号
uint16_t file_new_cluster(struct FCB *file, uint32_t count);

// 分配链接好的fat项
// 返回第一个簇号
uint16_t get_free_cluster_num(uint32_t count);

// 截断
// 文件扩大或缩小
int _truncate(struct FCB *, long fcb_offset, off_t offset);

// 
int adjust_cluster_count(struct FCB *, uint32_t count);

// 判断目录是否为空
// 空则返回1
int is_directory_empty(const struct FCB *);

// 是否是第一个空目录项
// 返回1表示当前是空目录
int is_entry_end(const struct FCB *);

// 判断目录项是否有效
// 返回1表示有效
int is_entry_exists(const struct FCB *);

#endif
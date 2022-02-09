#ifndef FAT16
#define FAT16

#define FUSE_USE_VERSION 31
#include<stddef.h>
#include<fuse3/fuse.h>

struct BPB {
    uint16_t bytes_per_sector;          // 扇区字节数，通常为 512
    uint8_t sectors_per_cluster;        // 每簇扇区数
    uint16_t reserved_sector;           // 保留扇区数，第一个FAT开始之前的扇区数，包括引导扇区
    uint8_t number_of_fat;              // FAT 表数量，一般为 2
    uint16_t root_entries;              // 根目录项数
    uint16_t small_sector;              // 小扇区数，超出 16 位表示范围，则用 large_sector 表示本分区扇区数
    uint8_t media_descriptor;           // 媒体描述符
    uint16_t sectors_per_fat;           // 每个 FAT 占用的扇区数
    uint16_t sectors_per_track;         // 每磁道扇区数
    uint16_t number_of_head;            // 磁头数
    uint32_t hidden_sector;             // 隐藏扇区数
    uint32_t large_sector;              // 大扇区数
}__attribute__((packed));

struct EBPB {
    uint8_t physical_drive_number;      // 物理驱动器号
    uint8_t reserved;                   // 保留，一般为 1
    uint8_t extended_boot_signature;    // 扩展引导标签
    uint32_t volume_serial_number;      // 卷序号
    char volume_label[11];              // 卷标
    char file_system_type[8];           // 文件系统类型，"FAT16"
}__attribute__((packed));

struct BootRecord {
    uint8_t jmp_boot[3];                // 跳转指令
    uint8_t oem_id[8];                  // OEM ID
    struct BPB bpb;
    struct EBPB ebpb;
    uint8_t boot_code[448];             // 引导程序代码
    uint16_t end_signature;             // 扇区结束标志 0xaa55
}__attribute__((packed));

// 目录表项
struct FCB {
    char filename[8];                // 文件名
    char extname[3];                 // 文件扩展名
    uint8_t metadata;                   // 属性
    uint8_t _[10];                      // 保留
    uint16_t last_modify_time;          // 文件最近修改时间
    uint16_t last_modify_date;          // 文件最近修改日期
    uint16_t first_cluster;             // 起始簇号
    uint32_t size;                      // 文件长度大小
}__attribute__((packed));

#define MAX_FILENAME sizeof(((struct FCB *)0)->filename)
#define MAX_EXTNAME sizeof(((struct FCB *)0)->extname)
#define MAX_FULLNAME (MAX_FILENAME+MAX_EXTNAME+1)

// 文件属性
#define META_READONLY       0b00000001
#define META_READ_WRITE     0b00000000
#define META_HIDDEN         0b00000010
#define META_SYSTEM         0b00000100
#define META_VOLUME_LABEL   0b00001000
#define META_DIRECTORY      0b00010000
#define META_ARCHIVE        0b00100000

#define ENT_NOTFOUND (-1)
#define ENT_END (-2)

// 簇号（FAT表项）
#define CLUSTER_FREE    0x0000  // 未分配的簇号
#define CLUSTER_MIN     0x0002  // 第一个可以用的簇号
#define CLUSTER_MAX     0xFFEF  // 最后一个可用的簇号
#define CLUSTER_END     0xffff  // 文件结束的簇号


extern struct BootRecord boot_record;
extern long offset_root;
extern long offset_fat;
extern long offset_data;
extern size_t size_fat;
extern size_t size_cluster;
extern size_t fcb_per_cluster;


//释放所有资源
void release();

// fuse {
    void *fat16_init (struct fuse_conn_info *conn, struct fuse_config *cfg);

    int fat16_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);

    int fat16_open(const char *path, struct fuse_file_info *fi);

    int fat16_opendir(const char *path, struct fuse_file_info *fi);

    int fat16_getattr(const char *path, struct stat *st, struct fuse_file_info *fi);

    int fat16_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);

    int fat16_write(const char *, const char *, size_t, off_t, struct fuse_file_info *);

    int fat16_flush(const char *, struct fuse_file_info *);
     
    int fat16_rename(const char *name, const char *new_name, unsigned int flags);

    int fat16_create(const char *path, mode_t mode, struct fuse_file_info *fi);
    
    int fat16_mkdir(const char *, mode_t);

    int fat16_rmdir(const char *);

    int fat16_truncate(const char *, off_t, struct fuse_file_info *);

    int fat16_chmod(const char *path, mode_t mode, struct fuse_file_info *fi);

    int fat16_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi);

    int fat16_access(const char *, int);

    int fat16_unlink(const char *);

    int fat16_release(const char *, struct fuse_file_info *);

    void fat16_destroy(void *private_data);
// }

#endif
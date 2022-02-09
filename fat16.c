#include "fat16.h"
#include "options.h"
#include "io.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

struct BootRecord boot_record;
long offset_root;
long offset_fat;
long offset_data;
size_t size_fat;
size_t size_cluster;
size_t fcb_per_cluster;

void *fat16_init (struct fuse_conn_info *conn, struct fuse_config *cfg) {

    cfg->kernel_cache = 1;

    fuse_log(FUSE_LOG_INFO, "fat16_init: image file %s\n",g_options.filename );

    // open image file
    if(init_myio(g_options.filename) < 0){
        fuse_log(FUSE_LOG_ERR, "FAT16 SYSTEM: failed to load image!");
        abort();
    }

    // read boot record
    if(sizeof(boot_record) != io_read(&boot_record, 0, sizeof(boot_record))){
        fuse_log(FUSE_LOG_ERR, "FAT16 SYSTEM: failed to load boot_record!");
        abort();
    }

    fuse_log(FUSE_LOG_DEBUG, "FAT16 SYSTEM: 扇区大小：%d\n", boot_record.bpb.bytes_per_sector);
    fuse_log(FUSE_LOG_DEBUG, "FAT16 SYSTEM: 每簇的块数：%d\n", boot_record.bpb.sectors_per_cluster);
    fuse_log(FUSE_LOG_DEBUG, "FAT16 SYSTEM: 保留扇区的数目：%d\n",boot_record.bpb.reserved_sector);
    fuse_log(FUSE_LOG_DEBUG, "FAT16 SYSTEM: 根分区目录项数：%d\n", boot_record.bpb.root_entries);

    offset_fat = boot_record.bpb.reserved_sector * boot_record.bpb.bytes_per_sector;        //FAT表起始点，保留扇区包括引导扇区
    size_fat = boot_record.bpb.bytes_per_sector * boot_record.bpb.sectors_per_fat;          //FAT表大小
    offset_root = offset_fat + size_fat * boot_record.bpb.number_of_fat;                    //根目录起始点
    size_cluster = boot_record.bpb.sectors_per_cluster * boot_record.bpb.bytes_per_sector;  //根目录大小
    offset_data = offset_root + boot_record.bpb.root_entries * sizeof(struct FCB);
    fcb_per_cluster = size_cluster / sizeof(struct FCB); //每簇的目录项

    fuse_log(FUSE_LOG_DEBUG, "FAT16 SYSTEM: FAT 偏移: %d\n", offset_fat);
    fuse_log(FUSE_LOG_DEBUG, "FAT16 SYSTEM: ROOT 偏移: %d\n", offset_root);

    return NULL;
}

void release()
{
	io_release();
}

int fat16_readdir(const char *path, 
    void *buf, 
    fuse_fill_dir_t filler, 
    off_t offset, 
    struct fuse_file_info *fi, 
    enum fuse_readdir_flags flags) {

    // 未使用的变量会报 warning
	(void) offset;
	(void) fi;
	(void) flags;
	fuse_log(FUSE_LOG_INFO, "FAT16 SYSTEM: readdir读取目录: %s \n", path);

    
    struct FCB fcb;
    int result;

    struct ReadDirOption opt = {
		.filler = filler,
		.buf = buf,
	};
    
    if (!strcmp(path, "/")) {   // 根目录
        result = traverse_root_dir(&opt, readdir_callback);
    } else {    // 子目录
        long ret;
        if ((ret = find_fcb(path, &fcb)) < 0) {
            return (int)ret;
        }

        result = traverse_sub_dir(&fcb, &opt, readdir_callback);
    }
    return 0;
}

int fat16_opendir(const char *path, struct fuse_file_info *fi) {
    fuse_log(FUSE_LOG_INFO, "FAT16 SYSTEM: opendir打开目录: %s\n", path);

	struct FCB fcb;

	if (!strcmp(path, "/")) {
		return 0;
	}

    long ret;
	if ((ret = find_fcb(path, &fcb)) < 0)
		return (int)ret;

	return 0;
}

int fat16_getattr(const char* path, struct stat* st, struct fuse_file_info* fi) {
	fuse_log(FUSE_LOG_INFO, "FAT16 SYSTEM:getattr获取属性: %s\n", path);

	struct FCB fcb;
	long result;

	if (!strcmp(path, "/")) {   // 根目录
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2;
	} else {
		if ((result = find_fcb(path, &fcb)) < 0)
			return (int)result;

        if ((fcb.metadata & META_VOLUME_LABEL))
            return -ENOENT;

		if ((fcb.metadata & META_DIRECTORY)) {  // 子目录
			st->st_mode = S_IFDIR | 0755;
			st->st_nlink = 2;
		} else {    // 普通文件
			st->st_mode = 0777 | S_IFREG;
			st->st_nlink = 1;
			st->st_size = fcb.size;
		}
	}

	return 0;
}

int fat16_open(const char* path, struct fuse_file_info* fi) {

	fuse_log(FUSE_LOG_INFO, "FAT16 SYSTEM: open打开: %s\n", path);

    if (fi->flags & O_CREAT) {
        fuse_log(FUSE_LOG_DEBUG, "open %s with O_CREAT\n", path);
    }

	struct FCB fcb;

	if (!strcmp(path, "/")) {
		return 0;
	}

	long ret;
	if ((ret = find_fcb(path, &fcb)) < 0)
		return -ENOENT;

    int result;
    if (fi->flags & O_TRUNC && 0 != (result = _truncate(&fcb, ret, 0))) {
        return result;
    }

	return 0;   // 找到文件
}


int fat16_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    fuse_log(FUSE_LOG_INFO, "FAT16 SYSTEM: read读取文件 %s\n", path);

    struct FCB fcb;

    if(find_fcb(path, &fcb) < 0) {
        return -ENOENT;
    }

    if (fcb.metadata & META_DIRECTORY)
        return -EISDIR;

    return read_file(&fcb, buf, offset, size);
}

int fat16_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    fuse_log(FUSE_LOG_INFO, "FAT16 SYSTEM: write写文件%s\n", path);

    if (strcmp(path, "/") == 0)
        return -EISDIR;

    struct FCB file;
    long result = find_fcb(path, &file);

    if (result < 0) // 文件不存在
        return -EIO;

    if (file.metadata & META_DIRECTORY)    // 不处理目录
        return -EISDIR;

    if (size > INT32_MAX)
        return -EINVAL;

    return write_file(&file, result, buf, offset, size);
}

int fat16_flush(const char *path, struct fuse_file_info *fi) {
    fuse_log(FUSE_LOG_INFO, "FAT16 SYSTEM: flush清空: %s\n", path);
    (void) fi;
    return 0;
}


int fat16_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    fuse_log(FUSE_LOG_INFO, "FAT16 SYSTEM: create创建文件: %s\n", path);

    (void) mode;
    (void) fi;

    if (strcmp(path, "/") == 0)
        return -EINVAL;

    struct FCB file;
    struct FCB parent_fcb;

    long result = find_fcb(path, &file);
    if (result >= 0) {   // 文件已存在
        return -EEXIST;
    }

    // 文件不存在
    char *tmp = strdup(path);
    if (!tmp)
        return -ENOMEM;
    char *parent = tmp; // 父目录
    char *name = strrchr(tmp, '/');
    if (name != NULL)
        *name++ = '\0';
    else
        name = tmp;

    // if (!is_filename_available(name)) // 判断文件名是否合法
    //     return -EINVAL;

    struct FindOption opt;  // 获取 pos 和 index

    // 查询可用空项
    if (*parent == '\0') {   // rootdir
        if ((result = traverse_root_dir(&opt, get_free_entry_callback)) < 0) {
            free(tmp);
            return (int)result;
        }
    } else {    // subdir
        long parent_offset;
        if ((parent_offset = find_fcb(parent, &parent_fcb)) < 0) {
            free(tmp);
            return (int)parent_offset;
        }

        if ((result = traverse_sub_dir(&parent_fcb, &opt, get_free_entry_callback)) < 0) {
            free(tmp);
            return (int)result;
        }

        if (opt.pos < 0) { // 给目录文件扩个容
            uint16_t new_cluster = file_new_cluster(&parent_fcb, 1);
            if (sizeof(struct FCB) != io_write(&parent_fcb, parent_offset, sizeof(struct FCB))) {
                free(tmp);
                return -EIO;
            }
            opt.pos = get_cluster_offset(new_cluster);
            opt.index = 0;
        }
    }

    if (opt.pos < 0) {// 目录项满了
        free(tmp);
        return -ENFILE;
    }

    // 提取文件名
    char *extname;
    char *dot = strrchr(name, '.');
    if (dot != NULL) {
        *dot = '\0';
        extname = dot + 1;
    } else {  // 无扩展名
        dot = name + strlen(name);
        extname = NULL;
    }

    if (dot - name > MAX_FILENAME || (extname != NULL && strlen(extname) > MAX_EXTNAME)) {
        free(tmp);
        return -EINVAL;
    }
    
    
    // 填充
    memset(&file, 0, sizeof(struct FCB));
    memset(file.filename, ' ', MAX_FILENAME);
    memset(file.extname, ' ', MAX_EXTNAME);
    memcpy(file.filename, name, strlen(name));
    if (extname)
        memcpy(file.extname, extname, strlen(extname));
    file.first_cluster = CLUSTER_END;

    // 写回
    if (sizeof(struct FCB) != io_write(&file, opt.pos + opt.index * sizeof(struct FCB), sizeof(struct FCB))) {
        return -EIO;
    }

    free(tmp);

    return 0;
}



int fat16_truncate(const char *path, off_t offset, struct fuse_file_info *fi) {
    fuse_log(FUSE_LOG_INFO, "FAT16 SYSTEM: truncate截断: %s\n", path);

    (void) fi;

    struct FCB file;
    long fcb_offset = find_fcb(path, &file);

    if (fcb_offset < 0)
       return -ENOENT;
    
    if (file.metadata & META_DIRECTORY)
        return -EISDIR;

    return _truncate(&file, fcb_offset, offset);
}


int fat16_rename(const char *name, const char *new_name, unsigned int flags) {
    fuse_log(FUSE_LOG_INFO, "FAT16 SYSTEM: rename重命名文件 %s -> %s\n", name, new_name);

    struct FCB file;
    struct FCB new_file;
    struct FCB parent_fcb;
    struct FindOption opt;  // 获取 pos 和 index
    long offset = find_fcb(name, &file);
    long new_offset = find_fcb(new_name, &new_file);


    if (new_offset > 0) { // 新目录或文件存在 
        if ((file.metadata & META_DIRECTORY && !is_directory_empty(&new_file))) {   // 非空目录不可覆盖
            return -ENOTEMPTY;
        } else {    // 移动&重命名
            // 释放将要被覆盖文件的内容
            release_cluster(new_file.first_cluster);

            char filename[MAX_FILENAME];
            char extname[MAX_EXTNAME];

            // 覆盖FCB后,修改文件名，同时将原FCB置\xe5
            memcpy(filename, &new_file.filename, MAX_FILENAME);
            memcpy(extname, &new_file.extname, MAX_EXTNAME);
            memcpy(&new_file, &file, sizeof(struct FCB));
            memcpy(&new_file.filename, filename, MAX_FILENAME);
            memcpy(&new_file.extname, extname, MAX_EXTNAME);
            file.filename[0] = '\xe5';

            // 写回
            if (sizeof(struct FCB) != io_write(&new_file, new_offset, sizeof(struct FCB))) {
                return -EIO;
            }
            if (sizeof(struct FCB) != io_write(&file, offset, sizeof(struct FCB))) {
                return -EIO;
            }
        }
    } else {    // 目录或文件不存在
        char *tmp = strdup(new_name);
        if (!tmp)
            return -ENOMEM;
        char *new_parent = tmp; // 父目录
        char *new_filename = strrchr(tmp, '/');
        if (new_filename != NULL)
            *new_filename++ = '\0';
        else
            new_filename = tmp;


        int result;

        if (*new_parent == '\0') { // 根目录
            if ((result = traverse_root_dir(&opt, get_free_entry_callback)) < 0) {
                free(tmp);
                return result;
            }
        } else {    // 子目录
            long parent_offset;
            if ((parent_offset = find_fcb(new_parent, &parent_fcb)) < 0) {
                free(tmp);
                return (int)parent_offset;
            }

            if ((result = traverse_sub_dir(&parent_fcb, &opt, get_free_entry_callback)) < 0 ) {
                free(tmp);
                return result;
            }

            if (opt.pos < 0) { // 给目录文件扩个容
                uint16_t new_cluster = file_new_cluster(&parent_fcb, 1);
                if (sizeof(struct FCB) != io_write(&parent_fcb, parent_offset, sizeof(struct FCB))) {
                    free(tmp);
                    return -EIO;
                }
                opt.pos = get_cluster_offset(new_cluster);
                opt.index = 0;
            }
        }

        if (opt.pos < 0) {// 目录项满了
            free(tmp);
            return -ENFILE;
        }


        // 提取文件名
        char *extname;
        char *dot = strrchr(new_filename, '.');
        if (dot != NULL) {
            *dot = '\0';
            extname = dot + 1;
        } else {  // 无扩展名
            dot = new_filename + strlen(new_filename);
            extname = NULL;
        }

        if (dot - new_filename > MAX_FILENAME || (extname != NULL && strlen(extname) > MAX_EXTNAME)) {
            free(tmp);
            return -EINVAL;
        }

        
        // 填充
        memcpy(&new_file, &file, sizeof(struct FCB));
        memset(new_file.filename, ' ', MAX_FILENAME);
        memset(new_file.extname, ' ', MAX_EXTNAME);
        memcpy(new_file.filename, new_filename, strlen(new_filename));
        if (extname)
            memcpy(new_file.extname, extname, strlen(extname));
        file.filename[0] = '\xe5';
        free(tmp);

        // 写回
        if (sizeof(struct FCB) != io_write(&new_file, opt.pos + opt.index * sizeof(struct FCB), sizeof(struct FCB))) {
            return -EIO;
        }
        if (sizeof(struct FCB) != io_write(&file, offset, sizeof(struct FCB))) {
            return -EIO;
        }
    }

    return 0;
}

int fat16_chmod(const char *path, mode_t mode, struct fuse_file_info *fi) {
    fuse_log(FUSE_LOG_INFO, "FAT16 SYSTEM:chmod: %s\n", path);

    (void) mode;
    (void) fi;

    return 0;
}

int fat16_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi) {
    fuse_log(FUSE_LOG_INFO, "FAT16 SYSTEM:chown: %s\n", path);

    (void) uid;
    (void) gid;
    (void) fi;

    return 0;
}


int my_statfs(const char *path, struct statvfs *sfs) {
    fuse_log(FUSE_LOG_INFO, "FAT16 SYSTEM:statfs: %s\n", path);

    (void) path;

    // memset(sfs, 0, sizeof(struct statvfs));
    // sfs->f_bsize = size_cluster;
    // sfs->f_frsize = sfs->f_bsize;
    // sfs->f_blocks = size_cluster / sfs->f_bsize;
    // sfs->f_namemax = MAX_FILENAME;
    // sfs->f_bfree = (DRIVE_SIZE - HEADER_SECTORS * boot_record.bpb.bytes_per_sector) / sfs->f_bsize;
    // sfs->f_bavail = sfs->f_bfree;
    // sfs->f_fsid = 0x1234;

    return 0;
}

int fat16_access(const char *path, int flags) {
    (void) path;
    (void) flags;

    return 0;
}


int fat16_unlink(const char *path) {
    fuse_log(FUSE_LOG_INFO, "FAT16 SYSTEM: unlink删除: %s\n", path);

    struct FCB file;
    long result;
    
    if ((result = find_fcb(path, &file)) < 0)
        return -ENOENT;

    if ((file.metadata & META_VOLUME_LABEL))
        return -ENOENT;

    if ((file.metadata & META_DIRECTORY))
        return -EISDIR;

    return remove_file(&file, result);
}


int fat16_release(const char *path, struct fuse_file_info *fi) {
    fuse_log(FUSE_LOG_INFO, "FAT16 SYSTEM: release释放打开的文件: %s\n", path);

    (void) fi;

    return 0;
}


void fat16_destroy(void *private_data) {
    (void) private_data;
    release();
    fuse_log(FUSE_LOG_INFO, "FAT16 SYSTEM: Image file has been stored! \n");
}

int fat16_mkdir(const char *path, mode_t mode) {
    fuse_log(FUSE_LOG_INFO, "FAT16 SYSTEM: mkdir创建目录: %s\n", path);

    (void) mode;

    if (strcmp(path, "/") == 0)
        return -EINVAL;

    struct FCB file;
    struct FCB parent_fcb;

    long result = find_fcb(path, &file);
    if (result >= 0) {   // 文件已存在
        return -EEXIST;
    }

    // 目录不存在
    char *tmp = strdup(path);
    if (!tmp)
        return -ENOMEM;
    char *parent = tmp; // 父目录
    char *name = strrchr(tmp, '/');
    if (name != NULL)
        *name++ = '\0';
    else
        name = tmp;

    if (!is_filename_available(name)) // 判断目录名是否合法
        return -EINVAL;

    struct FindOption opt;  // 获取 pos 和 index

    // 查询可用空项
    if (*parent == '\0') {   // rootdir
        if ((result = traverse_root_dir(&opt, get_free_entry_callback)) < 0) {
            free(tmp);
            return (int)result;
        }
    } else {    // subdir
        long parent_offset;
        if ((parent_offset = find_fcb(parent, &parent_fcb)) < 0) {
            free(tmp);
            return (int)parent_offset;
        }

        if ((result = traverse_sub_dir(&parent_fcb, &opt, get_free_entry_callback)) < 0) {
            free(tmp);
            return (int)result;
        }

        if (opt.pos < 0) { // 给目录文件扩个容
            uint16_t new_cluster = file_new_cluster(&parent_fcb, 1);
            if (sizeof(struct FCB) != io_write(&parent_fcb, parent_offset, sizeof(struct FCB))) {
                free(tmp);
                return -EIO;
            }
            opt.pos = get_cluster_offset(new_cluster);
            opt.index = 0;
        }
    }

    if (opt.pos < 0) {// 目录项满了
        free(tmp);
        return -ENFILE;
    }
    
    
    // 填充
    memset(&file, 0, sizeof(struct FCB));
    memset(file.filename, ' ', MAX_FILENAME);
    memset(file.extname, ' ', MAX_EXTNAME);
    memcpy(file.filename, name, strlen(name));
    file.metadata = file.metadata | META_DIRECTORY;
    file.first_cluster = CLUSTER_END;

    // 写回
    if (sizeof(struct FCB) != io_write(&file, opt.pos + opt.index * sizeof(struct FCB), sizeof(struct FCB))) {
        return -EIO;
    }

    free(tmp);
    return 0;
}

int fat16_rmdir(const char *path) {
    fuse_log(FUSE_LOG_INFO, "FAT16 SYSTEM: rmdir删除目录: %s\n", path);

    struct FCB file;
    long result;
    if((result = find_fcb(path, &file)) < 0) {
        return -ENOENT;
    }
    
    if ((file.metadata & META_VOLUME_LABEL))
        return -ENOENT;

    if (!(file.metadata & META_DIRECTORY))
        return -ENOTDIR;
    
    // 目录不为空不能删除
    if (!is_directory_empty(&file))
        return -ENOTEMPTY;

    return remove_file(&file, result);
}

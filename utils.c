#include "utils.h"
#include "fat16.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

long find_fcb(const char *path, struct FCB *ret) {
    char *tmp = strdup(path);
    if(!tmp) return -ENOMEM;

    char *name = strtok(tmp, "/");
    long result = -1;  // 目标fcb在image中的偏移

    int is_root = 1;
    struct FCB *fcb = NULL; // 存放目标fcb
    struct FindOption opt;

    // 查询目录
    while(name != NULL) {
        opt.name = name;
	    if (is_root) {   // 根目录
            is_root = 0;

            if ((result = traverse_root_dir(&opt, find_file_callback)) < 0) {
                break;
            }
            
        } else {    // 子目录
            if ((result = traverse_sub_dir(fcb, &opt, find_file_callback)) < 0) {
                break;
            }
        }

        if (opt.index < 0)    // 未找到
            break;

        fcb = &opt.fcb;
        name = strtok(NULL, "/");

    }

    if (result >= 0 && opt.index >= 0) { // 完整路径查找到了对应的 FCB
		result = opt.pos + sizeof(struct FCB) * opt.index; // FCB 的偏移
	} else {
		result = -ENOENT;
	}

    // 查找成功，填充ret
    if (fcb) {
        memcpy(ret, fcb, sizeof(*fcb));
    }

    free(tmp);
    return result;
}

void get_filename(const struct FCB *fcb, char *filename) {
    //memset(filename, '\0', sizeof(fcb->filename) + 1 + sizeof(fcb->extname) + 1);
    memcpy(filename, fcb->filename, sizeof(fcb->filename));
    
    int i;
    for(i = 0; i < sizeof(fcb->filename); i++) {
        if(filename[i] == ' ')
            break;
    };

    filename[i] = '.';
    memcpy(filename+i+1, fcb->extname, sizeof(fcb->extname));

    int j;
    for(j = 0; j < sizeof(fcb->extname); j++) {
        if(filename[ i+1+j ] == ' '){
            break;
        }
    }

    if(j == 0) {  // 没有扩展名
        filename[i] = '\x00';
    }

    filename[i+1+j]='\x00';
}

uint16_t next_cluster(uint16_t cluster) {
    long offset = offset_fat + cluster * sizeof(uint16_t);
    uint16_t ret = CLUSTER_END;

    if (cluster >= CLUSTER_MIN && 
        cluster <= CLUSTER_MAX && 
        cluster < (size_fat/ sizeof(uint16_t)) &&
        sizeof(ret) != io_read(&ret, offset, sizeof(ret))) {    // 目标项中存有下一簇的簇号
        return CLUSTER_END;
    }
    
    return ret;
}


long get_cluster_offset(uint16_t cluster) {
    if (cluster >= CLUSTER_MIN && 
        cluster <= CLUSTER_MAX && 
        cluster < (size_fat/ sizeof(uint16_t))) {
        return offset_data + size_cluster * (cluster - 2);
    }
    return -1;
}

int traverse_root_dir(void *opt,
    int (* callback)(void*, long, int, const struct FCB*)) {
    size_t entries = boot_record.bpb.root_entries;
    size_t number_of_cluster = (entries * sizeof(struct FCB) + size_cluster -1) / size_cluster;
    long pos = offset_root;

    struct FCB *dir = malloc(size_cluster);
	if (!dir) {
		return -ENOMEM;
	}

    // 读取root的所有簇
    for(int i = 0; i < number_of_cluster; i++) {
        if (size_cluster != io_read(dir, pos, size_cluster)){
            free(dir);
            return -ENODATA;
        }
                
        // 遍历簇
        for (int j = 0; j < fcb_per_cluster && j < entries; j++) {
            if (callback(opt, pos, j, &dir[j]) || dir[i].filename[0] == '\0') {
				free(dir);
				return 0;
			}
        }

        entries -= fcb_per_cluster;
        pos += size_cluster;
    }

    // 理论上不会执行到这
    callback(opt, -1, -1, NULL);
    
    free(dir);
    return 0;
}

int traverse_sub_dir(const struct FCB* fcb, void* opt,
	int (* callback)(void*, long, int, const struct FCB*))
{
	struct FCB *dir = malloc(size_cluster);
	if (!dir)
		return -ENOMEM;

	if (!(fcb->metadata & META_DIRECTORY)) { // 不是目录
		free(dir);
		return -ENOTDIR;
	} else {
		uint16_t cur = fcb->first_cluster;
		long pos = -1;
		while (cur != CLUSTER_END) {
			pos = get_cluster_offset(cur);
			if (pos == -1) {
				free(dir);
				return -ESPIPE;
			}

			if (size_cluster != io_read(dir, pos, size_cluster)) {
				free(dir);
				return -ENODATA;
			}

			for (int i = 0; i < fcb_per_cluster; i++) {
				if (callback(opt, pos, i, &dir[i]) || dir[i].filename[0] == '\0') {
					return 0;
				}
			}

			cur = next_cluster(cur);
		}

        // 目录为空
        callback(opt, -1, -1, NULL);
	}

    free(dir);
	return 0;
}

int find_file_callback(void *opt, long pos, int index, const struct FCB* fcb) {
	struct FindOption* f_opt = opt;
	char fullname[MAX_FULLNAME];

	if (!fcb || fcb->filename[0] == '\0') {  // 没找到
		f_opt->pos = -1;
		f_opt->index = ENT_END;

		return 1; // 终止遍历
	}

	get_filename(fcb, fullname);
	if (!strcasecmp(fullname, f_opt->name)) { // 找到了
		f_opt->pos = pos;
		f_opt->index = index;
		memcpy(&f_opt->fcb, fcb, sizeof(struct FCB));

		return 1;
	}

	return 0;
}

int readdir_callback(void* opt, long pos, int index, const struct FCB* fcb) {
	struct ReadDirOption *rd_opt = opt;
	if (!fcb)
        	return 1;

	if (fcb->filename[0] == '\xe5' || (fcb->metadata & META_VOLUME_LABEL))
		return 0;

	if (fcb->filename[0] == '\0')
		return 1;

	char fullname[MAX_FULLNAME];
	get_filename(fcb, fullname);

	rd_opt->filler(rd_opt->buf, fullname, NULL, 0, 0);

	return 0;
}

int get_free_entry_callback(void *opt, long pos, int index, const struct FCB *fcb) {
    struct FindOption* f_opt = opt;

    if (!fcb) {
        f_opt->pos = -1;
        f_opt->index -1;

        return -1;
    }

    if (fcb->filename[0] == '\0' || fcb->filename[0] == '\xe5') {  // 找到空闲目录项
		f_opt->pos = pos;
		f_opt->index = index;
		return 1;
	}

    return 0;
}


int read_file(const struct FCB *fcb, void *buff, off_t offset, size_t size) {
    fuse_log(FUSE_LOG_DEBUG, "read_file: file size = %d, offset = %d, size = %d\n", fcb->size, offset, size);
    if (offset >= fcb->size || size == 0) {
        return 0;
    }

    // 可用数据超出大小，更新size
    if (size > fcb->size || offset + size > fcb->size) { // 超出文件大小
        size = fcb->size - offset;
    }

    fuse_log(FUSE_LOG_DEBUG, "size after ajust: %d\n", size);


    // 定位到偏移对应的起始簇
    uint16_t cluster = fcb->first_cluster;
    while (offset >= size_cluster) {
        offset -= size_cluster;
        cluster = next_cluster(cluster);
        
        if (cluster < CLUSTER_MIN || cluster > CLUSTER_MAX) {
            fuse_log(FUSE_LOG_DEBUG, "invalid cluster %d\n", cluster);
            return -EIO;
        }
    }

    // 簇是否有效
    long cluster_offset = get_cluster_offset(cluster);
    if (cluster_offset < 0) {
        fuse_log(FUSE_LOG_DEBUG, "cluster_offset < 0 !\n");
        return -EIO;
    }


    int pos = 0;
    int n = size_cluster - offset;

    
    cluster_offset += offset;
    while (size > size_cluster) {
        if (n != io_read(buff + pos, cluster_offset, n))
            return -EIO;

        pos += n;
        n = size_cluster;
        size -= size_cluster;

        cluster = next_cluster(cluster);    // 获取下一簇号
        cluster_offset = get_cluster_offset(cluster);   // 获取簇偏移
        if (cluster_offset < 0) {
            fuse_log(FUSE_LOG_DEBUG, "invalid cluster %d, cluster_offset < 0 !\n", cluster);
            return -EIO;
        }
    }

    // 不足一簇
    if (size != io_read(buff + pos, cluster_offset, size)) 
        return -EIO;

    pos += size;

    return pos;
}


int write_file(struct FCB *fcb, long fcb_offset, void *buff, off_t offset, size_t length) {
    fuse_log(FUSE_LOG_DEBUG, "write_file: file size = %d, offset = %d, length = %d\n", fcb->size, offset, length);

    if (length == 0)
        return 0;

    if (offset + length < offset)  // 溢出了
        return -EINVAL;

    // 若文件为空，写入数据后占用簇的数量
    uint32_t write_cluster_count = (offset + length + size_cluster - 1) / size_cluster;

    // 原有文件大小占用的簇的数量
    uint32_t now_cluster_count = get_cluster_count(fcb);

    // 若文件为空，写入数据后文件的大小
    uint32_t write_size = offset + length;

    // 原有文件大小
    uint32_t now_size = fcb->size;

    // 需要扩容
    if (write_cluster_count > now_cluster_count) {
        if (CLUSTER_END == file_new_cluster(fcb, write_cluster_count - now_cluster_count))
            return -ENOSPC;
    }
    

    // 文件大小需要更改
    if (write_size > now_size)
        fcb->size = write_size;

    uint16_t cur = fcb->first_cluster;

    // 定位到偏移对应的起始簇
    while (offset >= size_cluster) {
        //assert(is_cluster_inuse(cur));
        cur = next_cluster(cur);
        offset -= size_cluster;
    }

    int pos = 0;
    long cluster_offset = get_cluster_offset(cur);

    int n = size_cluster - offset;
    cluster_offset += offset;
    while (length > size_cluster) {
        if (n != io_write(buff + pos, cluster_offset, n)) {
            fuse_log(FUSE_LOG_DEBUG, "Error line: %d, pos=%d, offset=%d, size=%d\n", __LINE__, pos, cluster_offset, n);
            return -EIO;
        }

        length -= n;
        pos +=n;

        cur = next_cluster(cur);
        cluster_offset = get_cluster_offset(cur);
        n = size_cluster;
    }
    

    // 不足一簇
    if (length != io_write(buff + pos, cluster_offset, length))
        return -EIO;

    if (sizeof(struct FCB) != io_write(fcb, fcb_offset, sizeof(struct FCB))) {
        return -EIO;
    }

    pos += length;
    return pos;
}


void release_cluster(uint16_t first_cluster) {
    uint16_t next = first_cluster;
    while (is_cluster_inuse(next)) {
        uint16_t buf = CLUSTER_FREE;
        if (sizeof(uint16_t) != io_write(&buf, offset_fat + next * sizeof(uint16_t), sizeof(uint16_t))) {
            abort();
        }
        
        next = next_cluster(next);
    }
}

int is_cluster_inuse(uint16_t cluster_num) {
    return CLUSTER_MIN <= cluster_num && cluster_num <= CLUSTER_MAX;
}

int remove_file(struct FCB *file, long offset_fcb) {
    release_cluster(file->first_cluster);
    file->filename[0] = '\xe5';
    // 更新fcb
    if (sizeof(struct FCB) != io_write(file, offset_fcb, sizeof(struct FCB))) {
        return -EIO;
    }
    return 0;
}

uint32_t get_cluster_count(struct FCB *fcb) {
    uint32_t count = 0;
    uint16_t cur = fcb->first_cluster;
    while (is_cluster_inuse(cur)) {
        count++;
        cur = next_cluster(cur);
    }

    return count;
}

int is_filename_available(const char *filename) {
    size_t len = strlen(filename);
    if (len > MAX_FILENAME || len == 0)
        return 0;

    for (size_t i = 0; i < len; i++) {
        if ((filename[i] >= 'a' && filename[i] <= 'z') ||
            (filename[i] >= 'A' && filename[i] <= 'Z') ||
            (filename[i] >= '0' && filename[i] <= '9') ||
            (filename[i] == '_')
                ) { // 仅支持数字字母下划线
            continue;
        } else {
            return 0;
        }
    }

    return 1;
}

uint16_t file_new_cluster(struct FCB *file, uint32_t count) {
    // 分配新的簇，并初始化
    uint16_t new_cluster = get_free_cluster_num(count);
    if (new_cluster == CLUSTER_END)  // 没有空间可用了
        return CLUSTER_END;

    uint16_t cur = new_cluster;
    char *zero = malloc(size_cluster);
    if (!zero) {
        release_cluster(cur);
        return CLUSTER_END;
    }

    memset(zero, 0, size_cluster);

    while (is_cluster_inuse(cur)) {
        long offset = get_cluster_offset(cur);
        if (size_cluster != io_write(zero, offset, size_cluster)) {
            release_cluster(new_cluster);
            return -EIO;
        }
        cur = next_cluster(cur);
    }

    if (is_cluster_inuse(file->first_cluster)) {
        cur = file->first_cluster;

        while (is_cluster_inuse(next_cluster(cur))) {
            cur = next_cluster(cur);
        }

        // 写回
        if (sizeof(uint16_t) != io_write(&new_cluster, offset_fat + cur * sizeof(uint16_t), sizeof(uint16_t))) {
            release_cluster(new_cluster);
            return -EIO;
        }
        
    } else {  // 从未分配
        file->first_cluster = new_cluster;
    }

    return new_cluster;
}

uint16_t get_free_cluster_num(uint32_t count) {
    if (count == 0)
        return CLUSTER_END;

    uint16_t first = CLUSTER_END;

    while (count--) {
        size_t i;
        for (i = 0; i < size_fat / sizeof(uint16_t); i++) {
            if (next_cluster(i) == CLUSTER_FREE && get_cluster_offset(i) >= 0) {
                if (sizeof(uint16_t) != io_write(&first, offset_fat + i * sizeof(uint16_t), sizeof(uint16_t))) {
                    return -EIO;
                }
                break;
            }
        }

        if (i == size_fat / sizeof(uint16_t)) {
            // 不足够分配所需的簇，释放之前分配的簇
            release_cluster(first);
            return CLUSTER_END;
        }
      
        first = i;  
    }

    return first;
}




int _truncate(struct FCB *file, long fcb_offset, off_t offset) {


    // 文件大小所需的簇的数量
//    uint32_t old_cluster_count = (file->size + CLUSTER_SIZE - 1) / CLUSTER_SIZE;

    // 截断后所需的簇的数量
    uint32_t new_cluster_count = (offset + size_cluster - 1) / size_cluster;

    uint32_t old_size = file->size;
    uint32_t new_size = offset;

    int ret;
    if ((ret = adjust_cluster_count(file, new_cluster_count)) < 0) {
        return ret;
    }

    if (old_size == new_size) {
        return 0;
    } else if (old_size < new_size) { // 文件大小增加
        // 把后面的内容覆盖为 0
        void *null_buf = malloc(new_size - old_size);
        if (null_buf == NULL)
            return -EFAULT;

        memset(null_buf, 0, new_size - old_size);
        int n;
        if (new_size - old_size != (n = write_file(file, fcb_offset, null_buf, old_size, new_size - old_size))) {
            free(null_buf);
            return n;
        }
        free(null_buf);
    } else { // 文件大小减少
        // 不用管
    }

    file->size = new_size;
    return 0;
}

int adjust_cluster_count(struct FCB *fcb, uint32_t new_count) {
    uint32_t old_count = get_cluster_count(fcb);

    if (old_count == new_count) {
        return 0;
    } else if (old_count > new_count) { // 缩减
        uint16_t cur = fcb->first_cluster;
        uint16_t pre = CLUSTER_END;
        uint32_t counter = new_count;
        while (counter > 0) {
            counter--;
            pre = cur;
            cur = next_cluster(cur);
        }

        if (pre == CLUSTER_END) {
            fcb->first_cluster = CLUSTER_END;
        } else {
            uint16_t buf = CLUSTER_FREE;
            if (sizeof(uint16_t) != io_write(&buf, offset_fat + pre * sizeof(uint16_t) , sizeof(uint16_t))) {
                return -EIO;
            }
        }

        release_cluster(cur);
    } else {    // 扩容
        if (CLUSTER_END == file_new_cluster(fcb, new_count - old_count)) {
            return -ENOSPC;
        }
    }

    return 0;
}

int is_directory_empty(const struct FCB *file) {
    uint32_t entries = size_cluster / sizeof(struct FCB);
    uint16_t cur = file->first_cluster;

    struct FCB *dir = (struct FCB *)malloc(size_cluster);
	if (!dir) {
        free(dir);
		return -ENOMEM;
	}

    int stop = 0;
    long offset_cluster;
    while (is_cluster_inuse(cur) && !stop) {
        offset_cluster = get_cluster_offset(cur);
        if (size_cluster != io_read(dir, offset_cluster, size_cluster)) {
            free(dir);
            return -EIO;
        }

        for (size_t i = 0; i < entries; i++) {
            if (is_entry_end(&dir[i])) {
                stop = 1;
                break;
            }

            if (dir[i].filename[0] != '.' && is_entry_exists(&dir[i])) {
                free(dir);
                return 0;
            }
        }

        cur = next_cluster(cur);
    }

    free(dir);
    return 1;
}

int is_entry_end(const struct FCB *fcb) {
    return fcb->filename[0] == '\0';
}

int is_entry_exists(const struct FCB *fcb) {
    return fcb->filename[0] != '\x20' && fcb->filename[0] != '\xe5';
}

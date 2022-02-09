# FAT16 flie system
OS experiment: a simple FAT16 file system

---

## 文件系统结构
![image](https://user-images.githubusercontent.com/57005028/153136471-9131a762-ef1f-4e17-a038-93e74ff43eb9.png)
![image](https://user-images.githubusercontent.com/57005028/153136595-5ecc69df-8527-4ea8-927e-8b985d4f27c4.png)
![image](https://user-images.githubusercontent.com/57005028/153136732-4af77046-8bbd-4055-8777-14181d372ba4.png)
![image](https://user-images.githubusercontent.com/57005028/153136759-bdeda234-962f-4d57-8545-3b1adbac3401.png)


---

## 测试样例
1. 需要建立 image 文件，执行命令`dd if=/dev/zero bs=512 count=1M of=fat16.img`
- block size = 512bit
- 块数 1M
- 用 0 填充  

2. 格式化 image 文件，FAT16表项 16bit，执行命令`mkfs.vfat -F 16 fat16.img`
3. 挂载并执行
![image](https://user-images.githubusercontent.com/57005028/153135948-4d8c378a-83c9-45b0-8331-f2f4809741cb.png)
4. 另起命令行步进到目标目录执行有关命令测试，同时可查看前一命令行输出的有关log判断执行过程有无错误
![image](https://user-images.githubusercontent.com/57005028/153138510-803678ec-642b-494e-ae48-8ab0ad5a1c9c.png)

---

 ## 参考文献 
 1. https://blog.csdn.net/feelabclihu/article/details/109396707 fuse 
 2. https://liuyehcf.github.io/2017/09/25/%E6%93%8D%E4%BD%9C%E7%B3%BB%E7%BB%9F%E5%8E%9F%E7%90%86-%E6%96%87%E4%BB%B6%E7%B3%BB%E7%BB%9F2/ file system related
 3. https://blog.csdn.net/yeruby/article/details/41978199?%3E brief introduction of FAT16 file system 
 4. https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system wiki

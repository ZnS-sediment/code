#ifndef DISK_MANAGER_H
#define DISK_MANAGER_H

#include <string>
#include <fstream>
#include "config.h"

class DiskManager
{
public:
    DiskManager();
    ~DiskManager();

    // 创建并初始化虚拟磁盘文件
    void createDisk();

    // 检查磁盘文件是否存在
    bool diskExists();

    // 读取指定块号的数据到缓冲区
    bool readBlock(int block_id, char *buf);

    // 将缓冲区的数据写入指定块号
    bool writeBlock(int block_id, const char *buf);

private:
    std::fstream disk_file; // 磁盘文件流
};

struct FD
{
    std::string path;
    int flags = 0;
    std::size_t offset = 0;
    bool in_use = false;

    FD() = default;
    FD(std::string p, int f, std::size_t off, bool in)
        : path(std::move(p)), flags(f), offset(off), in_use(in) {}
};

#endif // DISK_MANAGER_H

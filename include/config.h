#ifndef CONFIG_H
#define CONFIG_H

#include <string>

// ================== 磁盘配置 ==================
const int BLOCK_SIZE = 1024;              // 块大小 (1KB)
const int DISK_BLOCKS = 10240;            // 虚拟磁盘总块数 (10MB)
const std::string DISK_PATH = "disk.img"; // 虚拟磁盘文件路径

// ================== 文件系统布局配置 ==================
const int BOOT_BLOCK_COUNT = 1;    // 引导块数量
const int SUPER_BLOCK_COUNT = 1;   // 超级块数量
const int INODE_BITMAP_BLOCKS = 1; // inode位图所占块数
const int DATA_BITMAP_BLOCKS = 4;  // 数据块位图所占块数 (假设可以管理 4*1024*8 = 32768 个数据块)

const int INODE_SIZE = 128;                           // 每个inode的大小 (bytes)
const int INODES_PER_BLOCK = BLOCK_SIZE / INODE_SIZE; // 每个块可以存放的inode数量
const int INODE_AREA_BLOCKS = 128;                    // inode区域所占块数 (总共 128 * 8 = 1024 个inodes)

// 各区域起始块号
const int BOOT_BLOCK_START = 0;
const int SUPER_BLOCK_START = BOOT_BLOCK_START + BOOT_BLOCK_COUNT;
const int INODE_BITMAP_START = SUPER_BLOCK_START + SUPER_BLOCK_COUNT;
const int DATA_BITMAP_START = INODE_BITMAP_START + INODE_BITMAP_BLOCKS;
const int INODE_AREA_START = DATA_BITMAP_START + DATA_BITMAP_BLOCKS;
const int DATA_AREA_START = INODE_AREA_START + INODE_AREA_BLOCKS;

// 总inode数量
const int TOTAL_INODES = INODE_AREA_BLOCKS * INODES_PER_BLOCK;

// ================== Inode 配置 ==================
const int DIRECT_BLOCKS = 10;                            // 直接数据块指针数量
const int INDIRECT_BLOCK_1 = 1;                          // 一级间接数据块指针数量
const int POINTERS_PER_BLOCK = BLOCK_SIZE / sizeof(int); // 每个块中可以存放的指针数量

#endif // CONFIG_H

#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include <string>
#include <vector>
#include <ctime>
#include <cstddef>
#include "disk_manager.h"
#include "config.h"

// 文件类型
enum FileType
{
    REGULAR_FILE,
    DIRECTORY
};

// Inode 结构
struct Inode
{
    int i_id;                    // inode编号
    FileType i_type;             // 文件类型
    int i_size;                  // 文件大小 (bytes)
    int i_blocks;                // 文件所占块数
    time_t i_atime;              // 最后访问时间
    time_t i_mtime;              // 最后修改时间
    time_t i_ctime;              // 创建时间
    int i_direct[DIRECT_BLOCKS]; // 直接数据块指针
    int i_indirect1;             // 一级间接数据块指针
    // 为了简化，不实现二级间接指针
};

// 超级块 结构
struct SuperBlock
{
    int s_total_blocks;      // 总块数
    int s_total_inodes;      // 总inode数
    int s_free_blocks_count; // 空闲块数
    int s_free_inodes_count; // 空闲inode数
    int s_inode_bitmap_start;
    int s_data_bitmap_start;
    int s_inode_area_start;
    int s_data_area_start;
};

// 目录项 结构
struct DirEntry
{
    char d_name[252]; // 文件名
    int d_inode_id;   // inode号
};

class FileSystem
{
public:
    FileSystem();
    ~FileSystem();

    // 格式化文件系统
    void format();

    // 挂载文件系统 (加载超级块等信息)
    void mount();

    // 创建文件
    int createFile(const std::string &path);

    // 创建目录
    int createDirectory(const std::string &path);

    // 打开文件 (返回inode id)
    int openFile(const std::string &path);

    // 关闭文件 (当前实现中无需太多操作)
    void closeFile(int inode_id);

    // 读取文件
    int readFile(int inode_id, char *buf, int size, int offset);

    // 写入文件
    int writeFile(int inode_id, const char *buf, int size, int offset);

    // 删除文件
    int removeFile(const std::string &path);

    // 删除目录
    int removeDirectory(const std::string &path);

    // 删除文件或目录；recursive 为目录时递归；force 失败时静默；失败原因写入 err
    bool rm(const std::string &path, bool recursive, bool force, std::string &err);

    // 列出目录内容
    void listDirectory(const std::string &path);

    // 获取当前工作目录
    std::string getCurrentPath() const;

    // 切换目录
    void changeDirectory(const std::string &path);

    // 简化版 open 标志
    enum OpenFlag
    {
        O_RDONLY = 1 << 0,
        O_WRONLY = 1 << 1,
        O_RDWR = O_RDONLY | O_WRONLY,
        O_CREAT = 1 << 2,
        O_TRUNC = 1 << 3,
        O_APPEND = 1 << 4
    };

    // 简化系统调用接口
    int sys_create(const std::string &path);
    int sys_open(const std::string &path, int flags);
    std::ptrdiff_t sys_read(int fd, std::string &out, std::size_t count);
    std::ptrdiff_t sys_write(int fd, const std::string &data);
    int sys_close(int fd);

    int sys_mkdir(const std::string &path);
    int sys_rmdir(const std::string &path);
    int sys_rm(const std::string &path);
    int sys_ls(const std::string &path); // 直接打印，返回 0/非0

private:
    DiskManager disk;
    SuperBlock super_block;
    bool inode_bitmap[TOTAL_INODES];
    // 数据块位图较大，动态分配
    bool *data_bitmap;
    int current_dir_inode_id; // 当前目录的inode id

    // 内部辅助函数
    void loadSuperBlock();
    void saveSuperBlock();
    void loadBitmaps();
    void saveBitmaps();

    int allocInode();
    void freeInode(int inode_id);
    int allocDataBlock();
    void freeDataBlock(int block_id);

    Inode readInode(int inode_id);
    void writeInode(int inode_id, const Inode &inode);

    // 路径解析，返回最后一个组件的父目录inode id和最后一个组件名
    int resolvePath(const std::string &path, std::string &last_component);
    // 根据路径查找inode
    int findInodeByPath(const std::string &path);
    // 在指定目录inode下查找文件名对应的inode
    int findInDir(int dir_inode_id, const std::string &filename);
    // 在指定目录inode下添加目录项
    bool addDirEntry(int dir_inode_id, const std::string &filename, int new_inode_id);
    // 在指定目录inode下删除目录项
    bool removeDirEntry(int dir_inode_id, const std::string &filename);

    // --- 文件描述符管理 ---
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
    std::vector<FD> fd_table_;

    int alloc_fd_(const std::string &path, int flags, std::size_t offset);
    bool check_fd_(int fd) const;

    // --- 在这里添加缺失的声明 ---
    void truncateFileData_(Inode &inode);
    bool directoryIsEmpty_(const Inode &inode) const;

    // 提示：将下列“假定存在的操作”替换为你实际已有的底层方法
    bool fs_path_exists_(const std::string &path, bool *is_dir = nullptr) const;
    bool fs_create_file_(const std::string &path);
    bool fs_read_file_all_(const std::string &path, std::string &out) const;
    bool fs_write_file_all_(const std::string &path, const std::string &data, bool truncate);
    bool fs_mkdir_(const std::string &path);
    bool fs_rmdir_(const std::string &path);
    bool fs_rm_(const std::string &path);
    bool fs_list_dir_(const std::string &path, std::vector<std::string> &entries) const;

    // 工具：获取文件大小（通过顺序读）
    std::size_t get_file_size_(int inode) const;
};

#endif // FILE_SYSTEM_H

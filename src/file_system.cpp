#include "file_system.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <vector>
#include <sstream>

bool FileSystem::rm(const std::string &path, bool recursive, bool force, std::string &err)
{
    err.clear();
    if (path.empty())
    {
        if (!force)
            err = "invalid path";
        return force;
    }
    if (path == "/")
    {
        err = "cannot remove root directory";
        return false;
    }

    int inode_id = findInodeByPath(path);
    if (inode_id < 0)
    {
        if (force)
            return true;
        err = "No such file or directory";
        return false;
    }

    Inode node = readInode(inode_id);
    if (node.i_type == REGULAR_FILE)
    {
        int rc = removeFile(path);
        if (rc == 0)
            return true;
        if (!force)
            err = "remove file failed";
        return force;
    }

    if (node.i_type == DIRECTORY)
    {
        if (!recursive)
        {
            err = "Is a directory";
            return false;
        }

        // 收集子项名称（跳过 . 和 ..）
        std::vector<std::string> children;
        char block_buf[BLOCK_SIZE];
        for (int i = 0; i < DIRECT_BLOCKS && node.i_direct[i] != -1; ++i)
        {
            disk.readBlock(node.i_direct[i], block_buf);
            const DirEntry *entries = reinterpret_cast<const DirEntry *>(block_buf);
            const int entry_count = BLOCK_SIZE / static_cast<int>(sizeof(DirEntry));
            for (int j = 0; j < entry_count; ++j)
            {
                if (entries[j].d_inode_id == -1 || entries[j].d_name[0] == '\0')
                    continue;
                std::string name(entries[j].d_name);
                if (name == "." || name == "..")
                    continue;
                children.push_back(name);
            }
        }

        // 先递归删除子项
        for (const auto &name : children)
        {
            std::string child = (path == "/") ? ("/" + name) : (path + "/" + name);
            std::string child_err;
            if (!rm(child, true, force, child_err))
            {
                if (!force)
                {
                    err = child_err.empty() ? "failed to remove child" : child_err;
                    return false;
                }
            }
        }

        // 子项清空后删除目录自身
        int rc = removeDirectory(path);
        if (rc == 0)
            return true;
        if (!force)
            err = "directory remove failed";
        return force;
    }

    if (!force)
        err = "unknown inode type";
    return force;
}

FileSystem::FileSystem()
{
    data_bitmap = new bool[DISK_BLOCKS]; // 应该根据super_block信息来定大小
    if (disk.diskExists())
    {
        mount();
    }
    else
    {
        std::cout << "Disk not found. Formatting a new disk..." << std::endl;
        format();
    }
}

FileSystem::~FileSystem()
{
    // 在析构时可以考虑保存所有状态
    saveSuperBlock();
    saveBitmaps();
    delete[] data_bitmap;
}

void FileSystem::format()
{
    disk.createDisk();

    // 1. 初始化 SuperBlock
    super_block.s_total_blocks = DISK_BLOCKS;
    super_block.s_total_inodes = TOTAL_INODES;
    super_block.s_inode_bitmap_start = INODE_BITMAP_START;
    super_block.s_data_bitmap_start = DATA_BITMAP_START;
    super_block.s_inode_area_start = INODE_AREA_START;
    super_block.s_data_area_start = DATA_AREA_START;

    // 2. 初始化位图
    memset(inode_bitmap, 0, sizeof(inode_bitmap));
    memset(data_bitmap, 0, DISK_BLOCKS * sizeof(bool));

    // 标记系统占用的块
    for (int i = 0; i < DATA_AREA_START; ++i)
    {
        data_bitmap[i] = true;
    }
    super_block.s_free_blocks_count = DISK_BLOCKS - DATA_AREA_START;
    super_block.s_free_inodes_count = TOTAL_INODES;

    // 3. 创建根目录
    int root_inode_id = allocInode(); // 应该是0号inode
    if (root_inode_id != 0)
    {
        std::cerr << "Critical: Root inode id is not 0!" << std::endl;
    }
    Inode root_inode;
    root_inode.i_id = root_inode_id;
    root_inode.i_type = DIRECTORY;
    root_inode.i_size = 2 * sizeof(DirEntry); // . and ..
    root_inode.i_blocks = 1;
    root_inode.i_ctime = root_inode.i_mtime = root_inode.i_atime = time(NULL);
    root_inode.i_direct[0] = allocDataBlock();
    for (int i = 1; i < DIRECT_BLOCKS; ++i)
        root_inode.i_direct[i] = -1;
    root_inode.i_indirect1 = -1;

    writeInode(root_inode_id, root_inode);

    // 在根目录数据块中创建 . 和 ..
    char block_buf[BLOCK_SIZE];
    memset(block_buf, 0, BLOCK_SIZE);
    DirEntry *dir_entries = (DirEntry *)block_buf;

    strcpy(dir_entries[0].d_name, ".");
    dir_entries[0].d_inode_id = root_inode_id;

    strcpy(dir_entries[1].d_name, "..");
    dir_entries[1].d_inode_id = root_inode_id;
    int entry_count = BLOCK_SIZE / sizeof(DirEntry);
    for (int k = 2; k < entry_count; ++k)
    {
        dir_entries[k].d_inode_id = -1;
        dir_entries[k].d_name[0] = '\0';
    }
    disk.writeBlock(root_inode.i_direct[0], block_buf);

    // 删除原先多余的两行
    // inode.i_atime = inode.i_mtime = time(NULL);
    // writeInode(new_inode_id, inode);

    // 4. 写入磁盘
    saveSuperBlock();
    saveBitmaps();

    current_dir_inode_id = 0;
    std::cout << "Disk formatted successfully." << std::endl;
}

void FileSystem::mount()
{
    loadSuperBlock();
    loadBitmaps();
    current_dir_inode_id = 0; // 默认当前目录是根目录
    std::cout << "File system mounted." << std::endl;
}

// =================================================================
// 以下是各主要功能的简化实现，很多细节和错误处理被省略
// =================================================================

int FileSystem::createFile(const std::string &path)
{
    std::string filename;
    int parent_inode_id = resolvePath(path, filename);
    if (parent_inode_id < 0)
    {
        std::cerr << "Error: Invalid path." << std::endl;
        return -1;
    }
    if (findInDir(parent_inode_id, filename) >= 0)
    {
        std::cerr << "Error: File or directory already exists." << std::endl;
        return -1;
    }

    int new_inode_id = allocInode();
    if (new_inode_id < 0)
    {
        std::cerr << "Error: No free inode available." << std::endl;
        return -1;
    }

    Inode inode;
    inode.i_id = new_inode_id;
    inode.i_type = REGULAR_FILE;
    inode.i_size = 0;
    inode.i_blocks = 0;
    inode.i_ctime = inode.i_mtime = inode.i_atime = time(NULL);
    for (int i = 0; i < DIRECT_BLOCKS; ++i)
        inode.i_direct[i] = -1;
    inode.i_indirect1 = -1;

    writeInode(new_inode_id, inode);
    addDirEntry(parent_inode_id, filename, new_inode_id);

    return new_inode_id;
}

int FileSystem::createDirectory(const std::string &path)
{
    std::string dirname;
    int parent_inode_id = resolvePath(path, dirname);
    if (parent_inode_id < 0)
    {
        std::cerr << "Error: Invalid path." << std::endl;
        return -1;
    }
    if (findInDir(parent_inode_id, dirname) >= 0)
    {
        std::cerr << "Error: File or directory already exists." << std::endl;
        return -1;
    }

    int new_inode_id = allocInode();
    if (new_inode_id < 0)
    {
        std::cerr << "Error: No free inode available." << std::endl;
        return -1;
    }

    // 创建新目录的inode
    Inode inode;
    inode.i_id = new_inode_id;
    inode.i_type = DIRECTORY;
    inode.i_size = 2 * sizeof(DirEntry);
    inode.i_blocks = 1;
    inode.i_ctime = inode.i_mtime = inode.i_atime = time(NULL);
    inode.i_direct[0] = allocDataBlock();
    if (inode.i_direct[0] < 0)
    {
        freeInode(new_inode_id);
        std::cerr << "Error: No free data block available." << std::endl;
        return -1;
    }
    for (int i = 1; i < DIRECT_BLOCKS; ++i)
        inode.i_direct[i] = -1;
    inode.i_indirect1 = -1;
    writeInode(new_inode_id, inode);

    // 在父目录中添加条目
    addDirEntry(parent_inode_id, dirname, new_inode_id);

    // 在新目录的数据块中创建 . 和 ..
    char block_buf[BLOCK_SIZE];
    memset(block_buf, 0, BLOCK_SIZE);
    DirEntry *dir_entries = (DirEntry *)block_buf;
    strcpy(dir_entries[0].d_name, ".");
    dir_entries[0].d_inode_id = new_inode_id;
    strcpy(dir_entries[1].d_name, "..");
    dir_entries[1].d_inode_id = parent_inode_id;
    disk.writeBlock(inode.i_direct[0], block_buf);

    return new_inode_id;
}

void FileSystem::listDirectory(const std::string &path)
{
    int inode_id = findInodeByPath(path);
    if (inode_id < 0)
    {
        std::cerr << "Error: Directory not found." << std::endl;
        return;
    }
    Inode inode = readInode(inode_id);
    if (inode.i_type != DIRECTORY)
    {
        std::cerr << "Error: Not a directory." << std::endl;
        return;
    }

    char block_buf[BLOCK_SIZE];
    // 仅实现直接块的遍历
    for (int i = 0; i < DIRECT_BLOCKS && inode.i_direct[i] != -1; ++i)
    {
        disk.readBlock(inode.i_direct[i], block_buf);
        DirEntry *dir_entries = (DirEntry *)block_buf;
        int entry_count = BLOCK_SIZE / sizeof(DirEntry);
        for (int j = 0; j < entry_count; ++j)
        {
            if (dir_entries[j].d_inode_id != -1 && strlen(dir_entries[j].d_name) > 0)
            {
                Inode entry_inode = readInode(dir_entries[j].d_inode_id);
                if (entry_inode.i_type == DIRECTORY)
                {
                    std::cout << "d  " << dir_entries[j].d_name << "/" << std::endl;
                }
                else
                {
                    std::cout << "f  " << dir_entries[j].d_name << "  (" << entry_inode.i_size << " bytes)" << std::endl;
                }
            }
        }
    }
}

int FileSystem::writeFile(int inode_id, const char *buf, int size, int offset)
{
    Inode inode = readInode(inode_id);
    if (inode.i_type != REGULAR_FILE)
        return -1;

    int bytes_written = 0;
    char block_buf[BLOCK_SIZE];
    // 极度简化的写入，仅支持直接块，且不支持扩展文件
    while (bytes_written < size)
    {
        int block_idx = (offset + bytes_written) / BLOCK_SIZE;
        int block_offset = (offset + bytes_written) % BLOCK_SIZE;

        if (block_idx >= DIRECT_BLOCKS)
        {
            std::cerr << "Error: File size exceeds direct block limit (simplification)." << std::endl;
            break;
        }

        int physical_block = inode.i_direct[block_idx];
        if (physical_block == -1)
        {
            physical_block = allocDataBlock();
            if (physical_block < 0)
            {
                std::cerr << "Error: No space left on device." << std::endl;
                break;
            }
            inode.i_direct[block_idx] = physical_block;
            inode.i_blocks++;
        }

        disk.readBlock(physical_block, block_buf);
        int write_len = std::min(BLOCK_SIZE - block_offset, size - bytes_written);
        memcpy(block_buf + block_offset, buf + bytes_written, write_len);
        disk.writeBlock(physical_block, block_buf);

        bytes_written += write_len;
    }

    inode.i_size = std::max(inode.i_size, offset + bytes_written);
    inode.i_mtime = time(NULL);
    writeInode(inode_id, inode);

    return bytes_written;
}

int FileSystem::readFile(int inode_id, char *buf, int size, int offset)
{
    Inode inode = readInode(inode_id);
    if (inode.i_type != REGULAR_FILE)
        return -1;

    int bytes_read = 0;
    int read_size = std::min(size, inode.i_size - offset);
    if (read_size <= 0)
        return 0;

    char block_buf[BLOCK_SIZE];
    // 极度简化的读取，仅支持直接块
    while (bytes_read < read_size)
    {
        int block_idx = (offset + bytes_read) / BLOCK_SIZE;
        int block_offset = (offset + bytes_read) % BLOCK_SIZE;

        if (block_idx >= DIRECT_BLOCKS || inode.i_direct[block_idx] == -1)
        {
            break;
        }

        int physical_block = inode.i_direct[block_idx];
        disk.readBlock(physical_block, block_buf);
        int read_len = std::min(BLOCK_SIZE - block_offset, read_size - bytes_read);
        memcpy(buf + bytes_read, block_buf + block_offset, read_len);

        bytes_read += read_len;
    }

    inode.i_atime = time(NULL);
    writeInode(inode_id, inode);

    return bytes_read;
}

void FileSystem::changeDirectory(const std::string &path)
{
    int inode_id = findInodeByPath(path);
    if (inode_id < 0)
    {
        std::cerr << "Error: Directory not found." << std::endl;
        return;
    }
    Inode inode = readInode(inode_id);
    if (inode.i_type != DIRECTORY)
    {
        std::cerr << "Error: Not a directory." << std::endl;
        return;
    }
    current_dir_inode_id = inode_id;
}

std::string FileSystem::getCurrentPath() const
{
    if (current_dir_inode_id == 0)
    {
        return "/";
    }

    std::vector<std::string> path_components;
    int temp_inode_id = current_dir_inode_id;

    // 从当前目录向上回溯到根目录
    while (temp_inode_id != 0)
    {
        // 找到父目录的inode
        int parent_inode_id = const_cast<FileSystem *>(this)->findInDir(temp_inode_id, "..");
        if (parent_inode_id < 0 || parent_inode_id == temp_inode_id)
        {
            // 如果找不到父目录或父目录是自己（根目录的..），就中断
            break;
        }

        // 在父目录中查找当前目录的名字
        Inode parent_inode = const_cast<FileSystem *>(this)->readInode(parent_inode_id);
        char block_buf[BLOCK_SIZE];
        bool found_name = false;
        for (int i = 0; i < DIRECT_BLOCKS && parent_inode.i_direct[i] != -1; ++i)
        {
            const_cast<FileSystem *>(this)->disk.readBlock(parent_inode.i_direct[i], block_buf);
            DirEntry *entries = (DirEntry *)block_buf;
            int entry_count = BLOCK_SIZE / sizeof(DirEntry);
            for (int j = 0; j < entry_count; ++j)
            {
                if (entries[j].d_inode_id == temp_inode_id)
                {
                    path_components.push_back(entries[j].d_name);
                    found_name = true;
                    break;
                }
            }
            if (found_name)
                break;
        }

        temp_inode_id = parent_inode_id;
    }

    // 拼接路径
    std::string full_path = "";
    for (auto it = path_components.rbegin(); it != path_components.rend(); ++it)
    {
        full_path += "/" + *it;
    }

    return full_path.empty() ? "/" : full_path;
}

// =================================================================
// 以下是内部辅助函数的简化实现
// =================================================================

void FileSystem::loadSuperBlock()
{
    char buffer[BLOCK_SIZE];
    disk.readBlock(SUPER_BLOCK_START, buffer);
    memcpy(&super_block, buffer, sizeof(SuperBlock));
}

void FileSystem::saveSuperBlock()
{
    char buffer[BLOCK_SIZE];
    memset(buffer, 0, BLOCK_SIZE);
    memcpy(buffer, &super_block, sizeof(SuperBlock));
    disk.writeBlock(SUPER_BLOCK_START, buffer);
}

void FileSystem::loadBitmaps()
{
    char buffer[BLOCK_SIZE];
    disk.readBlock(INODE_BITMAP_START, buffer);
    memcpy(inode_bitmap, buffer, sizeof(inode_bitmap));

    for (int i = 0; i < DATA_BITMAP_BLOCKS; ++i)
    {
        disk.readBlock(DATA_BITMAP_START + i, buffer);
        memcpy((char *)data_bitmap + i * BLOCK_SIZE, buffer, BLOCK_SIZE);
    }
}

void FileSystem::saveBitmaps()
{
    char buffer[BLOCK_SIZE];
    memset(buffer, 0, BLOCK_SIZE);
    memcpy(buffer, inode_bitmap, sizeof(inode_bitmap));
    disk.writeBlock(INODE_BITMAP_START, buffer);

    for (int i = 0; i < DATA_BITMAP_BLOCKS; ++i)
    {
        memset(buffer, 0, BLOCK_SIZE);
        memcpy(buffer, (char *)data_bitmap + i * BLOCK_SIZE, BLOCK_SIZE);
        disk.writeBlock(DATA_BITMAP_START + i, buffer);
    }
}

int FileSystem::allocInode()
{
    for (int i = 0; i < TOTAL_INODES; ++i)
    {
        if (!inode_bitmap[i])
        {
            inode_bitmap[i] = true;
            super_block.s_free_inodes_count--;
            return i;
        }
    }
    return -1; // No free inode
}

int FileSystem::allocDataBlock()
{
    for (int i = DATA_AREA_START; i < DISK_BLOCKS; ++i)
    {
        if (!data_bitmap[i])
        {
            data_bitmap[i] = true;
            super_block.s_free_blocks_count--;
            return i;
        }
    }
    return -1; // No free data block
}

void FileSystem::freeDataBlock(int block_id)
{
    if (block_id < DATA_AREA_START || block_id >= DISK_BLOCKS)
        return;
    if (!data_bitmap[block_id])
        return;

    data_bitmap[block_id] = false;
    if (super_block.s_free_blocks_count < super_block.s_total_blocks)
        ++super_block.s_free_blocks_count;

    char zero[BLOCK_SIZE] = {0};
    disk.writeBlock(block_id, zero);
}

Inode FileSystem::readInode(int inode_id)
{
    Inode inode;
    int block_offset = inode_id / INODES_PER_BLOCK;
    int in_block_offset = inode_id % INODES_PER_BLOCK;
    char buffer[BLOCK_SIZE];
    disk.readBlock(INODE_AREA_START + block_offset, buffer);
    memcpy(&inode, buffer + in_block_offset * INODE_SIZE, sizeof(Inode));
    return inode;
}

void FileSystem::writeInode(int inode_id, const Inode &inode)
{
    int block_offset = inode_id / INODES_PER_BLOCK;
    int in_block_offset = inode_id % INODES_PER_BLOCK;
    char buffer[BLOCK_SIZE];
    disk.readBlock(INODE_AREA_START + block_offset, buffer);
    memcpy(buffer + in_block_offset * INODE_SIZE, &inode, sizeof(Inode));
    disk.writeBlock(INODE_AREA_START + block_offset, buffer);
}

int FileSystem::resolvePath(const std::string &path, std::string &last_component)
{
    if (path.empty())
        return -1;

    size_t last_slash = path.find_last_of('/');
    std::string parent_path;

    if (last_slash == std::string::npos)
    { // e.g. "file.txt"
        parent_path = ".";
        last_component = path;
    }
    else if (last_slash == 0)
    { // e.g. "/file.txt"
        parent_path = "/";
        last_component = path.substr(1);
    }
    else
    { // e.g. "/dir/file.txt"
        parent_path = path.substr(0, last_slash);
        last_component = path.substr(last_slash + 1);
    }

    return findInodeByPath(parent_path);
}

int FileSystem::findInodeByPath(const std::string &path)
{
    if (path.empty())
        return -1;

    int current_inode = (path[0] == '/') ? 0 : current_dir_inode_id;

    if (path == "/")
        return 0;
    if (path == ".")
        return current_dir_inode_id;
    if (path == "..")
    {
        return findInDir(current_dir_inode_id, "..");
    }

    std::string p = path;
    if (p[0] == '/')
        p = p.substr(1);

    size_t start = 0;
    size_t end = p.find('/');

    while (end != std::string::npos)
    {
        std::string component = p.substr(start, end - start);
        current_inode = findInDir(current_inode, component);
        if (current_inode < 0)
            return -1;
        Inode inode = readInode(current_inode);
        if (inode.i_type != DIRECTORY)
            return -1;
        start = end + 1;
        end = p.find('/', start);
    }

    std::string last_component = p.substr(start);
    return findInDir(current_inode, last_component);
}

int FileSystem::findInDir(int dir_inode_id, const std::string &filename)
{
    Inode dir_inode = readInode(dir_inode_id);
    if (dir_inode.i_type != DIRECTORY)
        return -1;

    char block_buf[BLOCK_SIZE];
    for (int i = 0; i < DIRECT_BLOCKS && dir_inode.i_direct[i] != -1; ++i)
    {
        disk.readBlock(dir_inode.i_direct[i], block_buf);
        DirEntry *entries = (DirEntry *)block_buf;
        int entry_count = BLOCK_SIZE / sizeof(DirEntry);
        for (int j = 0; j < entry_count; ++j)
        {
            if (entries[j].d_inode_id != -1 && strcmp(entries[j].d_name, filename.c_str()) == 0)
            {
                return entries[j].d_inode_id;
            }
        }
    }
    return -1;
}

bool FileSystem::addDirEntry(int dir_inode_id, const std::string &filename, int new_inode_id)
{
    Inode dir_inode = readInode(dir_inode_id);
    char block_buf[BLOCK_SIZE];

    for (int i = 0; i < DIRECT_BLOCKS; ++i)
    {
        int block_id = dir_inode.i_direct[i];
        if (block_id == -1)
        {
            block_id = allocDataBlock();
            if (block_id < 0)
                return false;
            dir_inode.i_direct[i] = block_id;
            dir_inode.i_blocks++;
            memset(block_buf, 0, BLOCK_SIZE);

            DirEntry *entries = reinterpret_cast<DirEntry *>(block_buf);
            int entry_count = BLOCK_SIZE / sizeof(DirEntry);
            for (int j = 0; j < entry_count; ++j)
            {
                entries[j].d_inode_id = -1;
                entries[j].d_name[0] = '\0';
            }
        }
        else
        {
            disk.readBlock(block_id, block_buf);
        }

        DirEntry *entries = reinterpret_cast<DirEntry *>(block_buf);
        int entry_count = BLOCK_SIZE / sizeof(DirEntry);
        for (int j = 0; j < entry_count; ++j)
        {
            if (entries[j].d_inode_id == -1 || strlen(entries[j].d_name) == 0)
            {
                entries[j].d_inode_id = new_inode_id;
                strcpy(entries[j].d_name, filename.c_str());
                disk.writeBlock(block_id, block_buf);
                dir_inode.i_size += sizeof(DirEntry);
                dir_inode.i_mtime = dir_inode.i_atime = time(NULL);
                writeInode(dir_inode_id, dir_inode);
                return true;
            }
        }
    }
    return false; // 目录满了（简化）
}

bool FileSystem::removeDirEntry(int dir_inode_id, const std::string &filename)
{
    Inode dir_inode = readInode(dir_inode_id);
    char block_buf[BLOCK_SIZE];

    for (int i = 0; i < DIRECT_BLOCKS && dir_inode.i_direct[i] != -1; ++i)
    {
        disk.readBlock(dir_inode.i_direct[i], block_buf);
        DirEntry *entries = reinterpret_cast<DirEntry *>(block_buf);
        int entry_count = BLOCK_SIZE / sizeof(DirEntry);
        for (int j = 0; j < entry_count; ++j)
        {
            if (entries[j].d_inode_id != -1 && strcmp(entries[j].d_name, filename.c_str()) == 0)
            {
                entries[j].d_inode_id = -1;
                entries[j].d_name[0] = '\0';
                disk.writeBlock(dir_inode.i_direct[i], block_buf);

                if (dir_inode.i_size >= static_cast<int>(sizeof(DirEntry)))
                    dir_inode.i_size -= sizeof(DirEntry);
                dir_inode.i_mtime = dir_inode.i_atime = time(NULL);
                writeInode(dir_inode_id, dir_inode);
                return true;
            }
        }
    }
    return false;
}

// 其他函数如 removeFile, removeDirectory, freeInode, freeDataBlock 等的实现被省略
// 它们需要递归地释放数据块和inode，并更新位图和超级块
int FileSystem::removeFile(const std::string &path)
{
    std::string filename;
    int parent_inode_id = resolvePath(path, filename);
    if (parent_inode_id < 0 || filename.empty())
    {
        std::cerr << "Error: Invalid path." << std::endl;
        return -1;
    }

    int inode_id = findInDir(parent_inode_id, filename);
    if (inode_id < 0)
    {
        std::cerr << "Error: File not found." << std::endl;
        return -1;
    }

    Inode inode = readInode(inode_id);
    if (inode.i_type != REGULAR_FILE)
    {
        std::cerr << "Error: Target is not a regular file." << std::endl;
        return -1;
    }

    truncateFileData_(inode);
    freeInode(inode_id);

    if (!removeDirEntry(parent_inode_id, filename))
        std::cerr << "Warning: directory entry cleanup failed." << std::endl;

    saveBitmaps();
    saveSuperBlock();
    return 0;
}

int FileSystem::removeDirectory(const std::string &path)
{
    if (path == "/" || path.empty())
    {
        std::cerr << "Error: cannot remove root directory." << std::endl;
        return -1;
    }

    std::string dirname;
    int parent_inode_id = resolvePath(path, dirname);
    if (parent_inode_id < 0 || dirname.empty())
    {
        std::cerr << "Error: Invalid path." << std::endl;
        return -1;
    }

    int inode_id = findInDir(parent_inode_id, dirname);
    if (inode_id < 0)
    {
        std::cerr << "Error: Directory not found." << std::endl;
        return -1;
    }

    Inode dir_inode = readInode(inode_id);
    if (dir_inode.i_type != DIRECTORY)
    {
        std::cerr << "Error: Target is not a directory." << std::endl;
        return -1;
    }

    if (!directoryIsEmpty_(dir_inode))
    {
        std::cerr << "Error: Directory not empty." << std::endl;
        return -1;
    }

    truncateFileData_(dir_inode); // 清空目录块
    freeInode(inode_id);

    if (!removeDirEntry(parent_inode_id, dirname))
        std::cerr << "Warning: directory entry cleanup failed." << std::endl;

    saveBitmaps();
    saveSuperBlock();
    return 0;
}

int FileSystem::alloc_fd_(const std::string &path, int flags, std::size_t offset)
{
    for (int i = 0; i < static_cast<int>(fd_table_.size()); ++i)
    {
        if (!fd_table_[i].in_use)
        {
            fd_table_[i] = FD{path, flags, offset, true};
            return i;
        }
    }
    fd_table_.push_back(FD{path, flags, offset, true});
    return static_cast<int>(fd_table_.size() - 1);
}

bool FileSystem::check_fd_(int fd) const
{
    return fd >= 0 && fd < static_cast<int>(fd_table_.size()) && fd_table_[fd].in_use;
}

// 下面这些 fs_* 方法是“适配层”，请用你已有的底层实现替换
// 例如：touch/echo/cat/ls 对应的内部方法。若名字不同，替换调用即可。
bool FileSystem::fs_path_exists_(const std::string &path, bool *is_dir) const
{
    auto *self = const_cast<FileSystem *>(this);
    int inode_id = self->findInodeByPath(path);
    if (inode_id < 0)
        return false;
    if (is_dir)
        *is_dir = (self->readInode(inode_id).i_type == DIRECTORY);
    return true;
}

bool FileSystem::fs_create_file_(const std::string &path)
{
    return createFile(path) >= 0;
}

bool FileSystem::fs_read_file_all_(const std::string &path, std::string &out) const
{
    auto *self = const_cast<FileSystem *>(this);
    int inode_id = self->findInodeByPath(path);
    if (inode_id < 0)
        return false;
    Inode inode = self->readInode(inode_id);
    if (inode.i_type != REGULAR_FILE)
        return false;

    out.clear();
    out.reserve(static_cast<std::size_t>(std::max(0, inode.i_size)));

    char block_buf[BLOCK_SIZE];
    int remaining = inode.i_size;
    while (remaining > 0)
    {
        int block_index = static_cast<int>(out.size() / BLOCK_SIZE);
        if (block_index >= DIRECT_BLOCKS)
            break;
        int block_id = inode.i_direct[block_index];
        if (block_id == -1)
            break;

        self->disk.readBlock(block_id, block_buf);
        int copy_len = std::min(remaining, BLOCK_SIZE);
        out.append(block_buf, block_buf + copy_len);
        remaining -= copy_len;
    }
    return remaining == 0;
}

bool FileSystem::fs_write_file_all_(const std::string &path, const std::string &data, bool truncate)
{
    int inode_id = findInodeByPath(path);
    if (inode_id < 0)
    {
        inode_id = createFile(path);
        if (inode_id < 0)
            return false;
    }
    Inode inode = readInode(inode_id);
    if (inode.i_type != REGULAR_FILE)
        return false;

    if (truncate)
        truncateFileData_(inode);

    int written = writeFile(inode_id, data.data(), static_cast<int>(data.size()), 0);
    bool ok = (written == static_cast<int>(data.size()));
    saveBitmaps();
    saveSuperBlock();
    return ok;
}

bool FileSystem::fs_mkdir_(const std::string &path)
{
    return createDirectory(path) >= 0;
}

bool FileSystem::fs_rmdir_(const std::string &path)
{
    return removeDirectory(path) == 0;
}

bool FileSystem::fs_rm_(const std::string &path)
{
    return removeFile(path) == 0;
}

bool FileSystem::fs_list_dir_(const std::string &path, std::vector<std::string> &entries) const
{
    entries.clear();
    auto *self = const_cast<FileSystem *>(this);
    int inode_id = self->findInodeByPath(path);
    if (inode_id < 0)
        return false;
    Inode inode = self->readInode(inode_id);
    if (inode.i_type != DIRECTORY)
        return false;

    char block_buf[BLOCK_SIZE];
    for (int i = 0; i < DIRECT_BLOCKS && inode.i_direct[i] != -1; ++i)
    {
        self->disk.readBlock(inode.i_direct[i], block_buf);
        const DirEntry *dir_entries = reinterpret_cast<const DirEntry *>(block_buf);
        int entry_count = BLOCK_SIZE / sizeof(DirEntry);
        for (int j = 0; j < entry_count; ++j)
        {
            if (dir_entries[j].d_inode_id != -1 && dir_entries[j].d_name[0] != '\0')
                entries.emplace_back(dir_entries[j].d_name);
        }
    }
    return true;
}

// ================= 简化系统调用实现 =================

int FileSystem::sys_create(const std::string &path)
{
    bool is_dir = false;
    if (fs_path_exists_(path, &is_dir))
        return -1; // 已存在
    return fs_create_file_(path) ? 0 : -1;
}

int FileSystem::sys_open(const std::string &path, int flags)
{
    bool is_dir = false;
    if (!fs_path_exists_(path, &is_dir))
    {
        if (flags & O_CREAT)
        {
            if (!fs_create_file_(path))
                return -1;
        }
        else
        {
            return -1;
        }
    }
    if (is_dir)
        return -1; // 不允许把目录当文件打开

    std::size_t offset = 0;
    if (flags & O_APPEND)
    {
        std::string content;
        if (!fs_read_file_all_(path, content))
            return -1;
        offset = content.size();
    }
    if (flags & O_TRUNC)
    {
        if (!fs_write_file_all_(path, "", true))
            return -1;
        offset = 0;
    }
    return alloc_fd_(path, flags, offset);
}

std::ptrdiff_t FileSystem::sys_read(int fd, std::string &out, std::size_t count)
{
    if (!check_fd_(fd))
        return -1;
    auto &f = fd_table_[fd];
    if (!(f.flags & O_RDONLY) && !(f.flags & O_RDWR))
        return -1;

    std::string content;
    if (!fs_read_file_all_(f.path, content))
        return -1;
    if (f.offset >= content.size())
    {
        out.clear();
        return 0;
    }
    std::size_t n = std::min(count, content.size() - f.offset);
    out.assign(content.data() + f.offset, n);
    f.offset += n;
    return static_cast<std::ptrdiff_t>(n);
}

std::ptrdiff_t FileSystem::sys_write(int fd, const std::string &data)
{
    if (!check_fd_(fd))
        return -1;
    auto &f = fd_table_[fd];
    if (!(f.flags & O_WRONLY) && !(f.flags & O_RDWR))
        return -1;

    std::string content;
    if (!fs_read_file_all_(f.path, content))
        content.clear();

    // 处理写入位置：覆盖/追加/中间写（简化为覆盖构造）
    if (f.flags & O_APPEND)
        f.offset = content.size();

    // 扩展到 offset
    if (f.offset > content.size())
        content.resize(f.offset, '\0');
    // 在 offset 处写入 data（覆盖）
    if (f.offset + data.size() > content.size())
    {
        content.replace(f.offset, std::string::npos, data);
    }
    else
    {
        content.replace(f.offset, data.size(), data);
    }
    f.offset += data.size();

    if (!fs_write_file_all_(f.path, content, /*truncate*/ true))
        return -1;
    return static_cast<std::ptrdiff_t>(data.size());
}

int FileSystem::sys_close(int fd)
{
    if (!check_fd_(fd))
        return -1;
    fd_table_[fd] = FD{}; // 置空
    return 0;
}

int FileSystem::sys_mkdir(const std::string &path)
{
    bool is_dir = false;
    if (fs_path_exists_(path, &is_dir))
        return -1;
    return fs_mkdir_(path) ? 0 : -1;
}

int FileSystem::sys_rmdir(const std::string &path)
{
    bool is_dir = false;
    if (!fs_path_exists_(path, &is_dir) || !is_dir)
        return -1;
    return fs_rmdir_(path) ? 0 : -1;
}

int FileSystem::sys_rm(const std::string &path)
{
    bool is_dir = false;
    if (!fs_path_exists_(path, &is_dir) || is_dir)
        return -1;
    return fs_rm_(path) ? 0 : -1;
}

int FileSystem::sys_ls(const std::string &path)
{
    // 直接复用已有打印逻辑，简单返回0
    listDirectory(path);
    return 0;
}

// 如果路径不存在则创建，再返回其 inode id
int FileSystem::openFile(const std::string &path)
{
    int inode_id = findInodeByPath(path);
    if (inode_id < 0)
    {
        // 尝试创建
        if (createFile(path) < 0)
            return -1;
        inode_id = findInodeByPath(path);
    }
    return inode_id;
}

// 简化实现：当前无需状态，直接返回
void FileSystem::closeFile(int /*inode_id*/)
{
    // no-op in this simplified FS
}

// 释放一个 inode（更新位图与超级块，并清空该 inode）
void FileSystem::freeInode(int inode_id)
{
    if (inode_id < 0 || inode_id >= super_block.s_total_inodes)
        return;
    if (!inode_bitmap[inode_id])
        return;

    inode_bitmap[inode_id] = false;
    if (super_block.s_free_inodes_count < super_block.s_total_inodes)
    {
        ++super_block.s_free_inodes_count;
    }

    Inode z{};
    z.i_id = inode_id;
    z.i_type = REGULAR_FILE;
    z.i_size = 0;
    z.i_blocks = 0;
    z.i_atime = z.i_mtime = z.i_ctime = 0;
    z.i_indirect1 = -1;
    for (int i = 0; i < DIRECT_BLOCKS; ++i)
        z.i_direct[i] = -1;

    writeInode(inode_id, z);
    saveBitmaps();
    saveSuperBlock();
}

void FileSystem::truncateFileData_(Inode &inode)
{
    for (int i = 0; i < DIRECT_BLOCKS; ++i)
    {
        if (inode.i_direct[i] != -1)
        {
            freeDataBlock(inode.i_direct[i]);
            inode.i_direct[i] = -1;
        }
    }
    inode.i_blocks = 0;
    inode.i_size = 0;
    inode.i_mtime = inode.i_atime = time(NULL);
    writeInode(inode.i_id, inode);
}

bool FileSystem::directoryIsEmpty_(const Inode &inode) const
{
    if (inode.i_type != DIRECTORY)
        return false;
    char block_buf[BLOCK_SIZE];
    auto *self = const_cast<FileSystem *>(this);
    for (int i = 0; i < DIRECT_BLOCKS && inode.i_direct[i] != -1; ++i)
    {
        self->disk.readBlock(inode.i_direct[i], block_buf);
        const DirEntry *entries = reinterpret_cast<const DirEntry *>(block_buf);
        int entry_count = BLOCK_SIZE / sizeof(DirEntry);
        for (int j = 0; j < entry_count; ++j)
        {
            if (entries[j].d_inode_id == -1 || entries[j].d_name[0] == '\0')
                continue;
            if (strcmp(entries[j].d_name, ".") == 0 || strcmp(entries[j].d_name, "..") == 0)
                continue;
            return false;
        }
    }
    return true;
}

#if 0
bool FileSystem::rm(const std::string &path, bool recursive, bool force, std::string &err)
{
    err.clear();
    if (path.empty())
    {
        if (!force)
            err = "invalid path";
        return force;
    }
    if (path == "/")
    {
        err = "cannot remove root directory";
        return false;
    }

    int inode_id = findInodeByPath(path);
    if (inode_id < 0)
    {
        if (force)
            return true;
        err = "No such file or directory";
        return false;
    }

    Inode node = readInode(inode_id);
    if (node.i_type == REGULAR_FILE)
    {
        int rc = removeFile(path);
        if (rc == 0)
            return true;
        if (!force)
            err = "remove file failed";
        return force;
    }

    if (node.i_type == DIRECTORY)
    {
        if (!recursive)
        {
            err = "Is a directory";
            return false;
        }

        // 收集子项名称（跳过 . 和 ..）
        std::vector<std::string> children;
        char block_buf[BLOCK_SIZE];
        for (int i = 0; i < DIRECT_BLOCKS && node.i_direct[i] != -1; ++i)
        {
            disk.readBlock(node.i_direct[i], block_buf);
            const DirEntry *entries = reinterpret_cast<const DirEntry *>(block_buf);
            const int entry_count = BLOCK_SIZE / static_cast<int>(sizeof(DirEntry));
            for (int j = 0; j < entry_count; ++j)
            {
                if (entries[j].d_inode_id == -1 || entries[j].d_name[0] == '\0')
                    continue;
                std::string name(entries[j].d_name);
                if (name == "." || name == "..")
                    continue;
                children.push_back(name);
            }
        }

        // 先递归删除子项
        for (const auto &name : children)
        {
            std::string child = (path == "/") ? ("/" + name) : (path + "/" + name);
            std::string child_err;
            if (!rm(child, true, force, child_err))
            {
                if (!force)
                {
                    err = child_err.empty() ? "failed to remove child" : child_err;
                    return false;
                }
            }
        }

        // 子项清空后删除目录自身
        int rc = removeDirectory(path);
        if (rc == 0)
            return true;
        if (!force)
            err = "directory remove failed";
        return force;
    }

    if (!force)
        err = "unknown inode type";
    return force;
}
#endif

#if 0
bool FileSystem::rm(const std::string &path, bool recursive, bool force, std::string &err)
{
    err.clear();
    if (path.empty())
    {
        if (!force)
            err = "invalid path";
        return force;
    }
    if (path == "/")
    {
        err = "cannot remove root directory";
        return false;
    }

    int inode_id = findInodeByPath(path);
    if (inode_id < 0)
    {
        if (force)
            return true;
        err = "No such file or directory";
        return false;
    }

    Inode node = readInode(inode_id);
    if (node.i_type == REGULAR_FILE)
    {
        int rc = removeFile(path);
        if (rc == 0)
            return true;
        if (!force)
            err = "remove file failed";
        return force;
    }

    if (node.i_type == DIRECTORY)
    {
        if (!recursive)
        {
            err = "Is a directory";
            return false;
        }

        // 收集子项名称（跳过 . 和 ..）
        std::vector<std::string> children;
        char block_buf[BLOCK_SIZE];
        for (int i = 0; i < DIRECT_BLOCKS && node.i_direct[i] != -1; ++i)
        {
            disk.readBlock(node.i_direct[i], block_buf);
            const DirEntry *entries = reinterpret_cast<const DirEntry *>(block_buf);
            const int entry_count = BLOCK_SIZE / static_cast<int>(sizeof(DirEntry));
            for (int j = 0; j < entry_count; ++j)
            {
                if (entries[j].d_inode_id == -1 || entries[j].d_name[0] == '\0')
                    continue;
                std::string name(entries[j].d_name);
                if (name == "." || name == "..")
                    continue;
                children.push_back(name);
            }
        }

        // 先递归删除子项
        for (const auto &name : children)
        {
            std::string child = (path == "/") ? ("/" + name) : (path + "/" + name);
            std::string child_err;
            if (!rm(child, true, force, child_err))
            {
                if (!force)
                {
                    err = child_err.empty() ? "failed to remove child" : child_err;
                    return false;
                }
            }
        }

        // 子项清空后删除目录自身
        int rc = removeDirectory(path);
        if (rc == 0)
            return true;
        if (!force)
            err = "directory remove failed";
        return force;
    }

    if (!force)
        err = "unknown inode type";
    return force;
}
#endif

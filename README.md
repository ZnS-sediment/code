# 简易文件系统设计与实现

这是一个基于C++的简单文件系统模拟项目，旨在实现操作系统课程设计的要求。

## 项目结构

- `include/`: 存放所有头文件。
  - `config.h`: 全局配置（磁盘大小、块大小等）。
  - `disk_manager.h`: 磁盘模拟层接口。
  - `file_system.h`: 文件系统核心层接口和数据结构。
  - `shell.h`: 用户交互Shell层接口。
- `src/`: 存放所有源文件。
  - `disk_manager.cpp`: 磁盘模拟层实现。
  - `file_system.cpp`: 文件系统核心层实现。
  - `shell.cpp`: 用户交互Shell层实现。
  - `main.cpp`: 程序主入口。
- `build/`: 存放编译生成的目标文件。
- `Makefile`: 用于编译项目。
- `disk.img`: 模拟的磁盘文件。
- `myfs`: 编译生成的可执行文件。

## 功能特性

- **分层设计**:
  1.  **磁盘层**: `DiskManager` 类通过读写一个大文件 (`disk.img`) 来模拟块设备。
  2.  **文件系统层**: `FileSystem` 类管理文件系统的元数据，包括超级块、inode、位图等，并实现了核心的文件和目录操作逻辑。
  3.  **用户接口层**: `Shell` 类提供了一个交互式的命令行界面，解析用户输入并调用文件系统层的功能。

- **磁盘布局**:
  - 引导块 (Boot Block)
  - 超级块 (Super Block)
  - Inode 位图 (Inode Bitmap)
  - 数据块位图 (Data Block Bitmap)
  - Inode 区 (Inode Area)
  - 数据区 (Data Area)

- **Inode 设计**:
  - 包含文件类型、大小、时间戳等元数据。
  - 使用直接指针和一级间接指针来索引数据块。

- **已实现命令**:
  - `format`: 格式化虚拟磁盘。
  - `ls [path]`: 列出目录内容。
  - `cd <path>`: 切换当前目录。
  - `mkdir <dirname>`: 创建新目录。
  - `touch <filename>`: 创建空文件。
  - `echo "text" > <file>`: 向文件写入文本（会覆盖）。
  - `cat <filename>`: 显示文件内容。
  - `help`: 显示帮助信息。
  - `exit`: 退出。

## 如何编译和运行

本项目建议在 Linux 或类 Unix 环境（如 WSL）下使用 `make` 进行编译。

1.  **编译项目**:
    在项目根目录下，打开终端并执行：
    ```bash
    make
    ```
    或者
    ```bash
    make all
    ```
    这将会编译所有源文件，并在根目录下生成一个名为 `myfs` 的可执行文件。

2.  **运行 Shell**:
    执行以下命令启动文件系统命令行：
    ```bash
    ./myfs
    ```
    程序启动时会自动检查 `disk.img` 文件。如果不存在，它将自动执行格式化操作。

3.  **清理生成文件**:
    如果需要清理所有编译生成的文件和虚拟磁盘，可以执行：
    ```bash
    make clean
    ```

## 示例操作

```bash
$ ./myfs
Starting Simple File System...
Disk not found. Formatting a new disk...
Disk formatted successfully.
File system mounted.
MyFS:/$ ls
d  ./
d  ../
MyFS:/$ mkdir home
MyFS:/$ mkdir bin
MyFS:/$ ls
d  ./
d  ../
d  home/
d  bin/
MyFS:/$ cd home
MyFS:/$ ls
d  ./
d  ../
MyFS:/$ touch hello.txt
MyFS:/$ ls
d  ./
d  ../
f  hello.txt  (0 bytes)
MyFS:/$ echo "hello world from simple fs" > hello.txt
MyFS:/$ ls
d  ./
d  ../
f  hello.txt  (26 bytes)
MyFS:/$ cat hello.txt
hello world from simple fs
MyFS:/$ exit
Exiting SimpleFS shell.
```

## 注意事项

- 本项目是一个**简化**的实现，主要用于教学和演示目的。
- `rm` 和 `rmdir` 命令尚未完全实现，因为它们需要递归地释放资源，逻辑较为复杂。
- 错误处理和边界条件检查还不够完善。
- 并发访问控制（高级要求）未实现。
- 路径解析功能比较基础，可能无法处理所有复杂的路径情况。
```
# code

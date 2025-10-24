#ifndef SHELL_H
#define SHELL_H

#include "file_system.h"
#include <string>
#include <vector>

class Shell
{
public:
    Shell(FileSystem *fs);
    void run();
    void executeCommandPublic(const std::string &command_line);

private:
    FileSystem *fs;
    void printPrompt();
    void executeCommand(const std::string &command_line);
    std::vector<std::string> split(const std::string &s, char delimiter);

    // 命令处理函数
    void handle_ls(const std::vector<std::string> &args);
    void handle_mkdir(const std::vector<std::string> &args);
    void handle_cd(const std::vector<std::string> &args);
    void handle_touch(const std::vector<std::string> &args); // touch == create
    void handle_rm(const std::vector<std::string> &args);
    void handle_rmdir(const std::vector<std::string> &args);
    // echo 解析较特殊，提供两个重载：
    // 1) 直接传入原始命令行（推荐）
    void handle_echo(const std::string &command_line); // echo "content" > file
    // 2) 兼容旧签名：传入分词结果，内部重建命令行后复用上面实现
    void handle_echo(const std::vector<std::string> &args);
    void handle_cat(const std::vector<std::string> &args); // cat == read
    void handle_format(const std::vector<std::string> &args);
    void handle_help(const std::vector<std::string> &args);
    void handle_exit(const std::vector<std::string> &args);
};

#endif // SHELL_H

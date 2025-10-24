#include "shell.h"
#include "file_system.h"
#include <iostream>
#include <sstream>
#include <algorithm>

// 在第一次使用 parse_int 之前加入前置声明
static int parse_int(const std::string &s);
// 实现：支持十进制/0x十六进制，失败返回 0
static int parse_int(const std::string &s)
{
    try
    {
        size_t idx = 0;
        int base = (s.size() > 2 && (s[0] == '0') && (s[1] == 'x' || s[1] == 'X')) ? 16 : 10;
        int v = std::stoi(s, &idx, base);
        if (idx != s.size())
            throw std::invalid_argument("extra");
        return v;
    }
    catch (...)
    {
        return 0;
    }
}

Shell::Shell(FileSystem *fs) : fs(fs) {}

void Shell::run()
{
    std::string line;
    while (true)
    {
        printPrompt();
        if (!std::getline(std::cin, line))
        {
            break; // EOF
        }
        if (line.empty())
        {
            continue;
        }
        executeCommand(line);
    }
}

void Shell::executeCommandPublic(const std::string &command_line)
{
    executeCommand(command_line);
}

void Shell::printPrompt()
{
    std::cout << "\033]0;LCX`s terminal\007";
    std::cout << "LCX`s terminal:" << fs->getCurrentPath() << "$ " << std::flush;
}

std::vector<std::string> Shell::split(const std::string &s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

// 简单工具：拆分参数
static std::vector<std::string> split_args(const std::string &s)
{
    std::istringstream iss(s);
    std::vector<std::string> v;
    std::string t;
    while (iss >> t)
        v.push_back(t);
    return v;
}

void Shell::executeCommand(const std::string &command_line)
{
    auto parts = split(command_line, ' ');
    if (parts.empty())
        return;

    const std::string &command = parts[0];

    if (command == "ls")
    {
        handle_ls(parts);
    }
    else if (command == "mkdir")
    {
        handle_mkdir(parts);
    }
    else if (command == "cd")
    {
        handle_cd(parts);
    }
    else if (command == "touch")
    {
        handle_touch(parts);
    }
    else if (command == "rm")
    {
        bool recursive = false, force = false;
        std::vector<std::string> paths;
        for (size_t i = 1; i < parts.size(); ++i)
        {
            const std::string &a = parts[i];
            if (!a.empty() && a[0] == '-')
            {
                for (size_t k = 1; k < a.size(); ++k)
                {
                    if (a[k] == 'r' || a[k] == 'R')
                        recursive = true;
                    else if (a[k] == 'f' || a[k] == 'F')
                        force = true;
                    else if (!force)
                        std::cout << "rm: unknown option -" << a[k] << "\n";
                }
            }
            else
            {
                paths.push_back(a);
            }
        }

        if (paths.empty())
        {
            std::cout << "usage: rm [-r] [-f] path...\n";
        }
        else
        {
            for (const auto &p : paths)
            {
                // 先尝试按文件删
                int rc_file = fs->removeFile(p);
                if (rc_file == 0)
                    continue;

                // 文件删除失败；若 -r，则再尝试目录
                if (recursive)
                {
                    int rc_dir = fs->removeDirectory(p); // 底层通常要求目录为空
                    if (rc_dir == 0)
                        continue;
                    if (!force)
                        std::cout << "rm: cannot remove '" << p << "': directory not empty or remove failed\n";
                }
                else
                {
                    if (!force)
                        std::cout << "rm: cannot remove '" << p << "': is a directory or remove failed\n";
                }
            }
        }
    }
    else if (command == "rmdir")
    {
        handle_rmdir(parts);
    }
    else if (command == "echo")
    {
        handle_echo(command_line); // echo需要特殊处理
    }
    else if (command == "cat")
    {
        handle_cat(parts);
    }
    else if (command == "format")
    {
        handle_format(parts);
    }
    else if (command == "help")
    {
        handle_help(parts);
    }
    else if (command == "exit")
    {
        handle_exit(parts);
    }
    else if (command == "create")
    {
        if (parts.size() < 2)
        {
            std::cout << "usage: create <path>\n";
        }
        else
        {
            int rc = fs->sys_create(parts[1]);
            std::cout << (rc == 0 ? "ok\n" : "err\n");
        }
    }
    else if (command == "open")
    {
        if (parts.size() < 2)
        {
            std::cout << "usage: open <path> [flags]\n";
        }
        else
        {
            int flags = FileSystem::O_RDWR;
            if (parts.size() >= 3)
                flags = parse_int(parts[2]); // 例如: 1=RDONLY, 2=WRONLY, 3=RDWR | 4=CREAT | 8=TRUNC | 16=APPEND
            int fd = fs->sys_open(parts[1], flags);
            if (fd < 0)
                std::cout << "err\n";
            else
                std::cout << "fd=" << fd << "\n";
        }
    }
    else if (command == "read")
    {
        if (parts.size() < 3)
        {
            std::cout << "usage: read <fd> <n>\n";
        }
        else
        {
            int fd = parse_int(parts[1]);
            std::size_t n = static_cast<std::size_t>(parse_int(parts[2]));
            std::string out;
            auto r = fs->sys_read(fd, out, n);
            if (r < 0)
                std::cout << "err\n";
            else
                std::cout << out << "\n";
        }
    }
    else if (command == "write")
    {
        if (parts.size() < 3)
        {
            std::cout << "usage: write <fd> <text>\n";
        }
        else
        {
            int fd = parse_int(parts[1]);
            std::string data = command_line.substr(command_line.find(parts[2])); // 简化提取剩余文本
            auto r = fs->sys_write(fd, data);
            std::cout << (r < 0 ? "err\n" : "ok\n");
        }
    }
    else if (command == "close")
    {
        if (parts.size() < 2)
        {
            std::cout << "usage: close <fd>\n";
        }
        else
        {
            int fd = parse_int(parts[1]);
            int rc = fs->sys_close(fd);
            std::cout << (rc == 0 ? "ok\n" : "err\n");
        }
    }
    else
    {
        std::cerr << "Unknown command: " << command << std::endl;
    }
}

void Shell::handle_ls(const std::vector<std::string> &args)
{
    std::string path = (args.size() > 1) ? args[1] : ".";
    fs->listDirectory(path);
}

void Shell::handle_mkdir(const std::vector<std::string> &args)
{
    if (args.size() < 2)
    {
        std::cerr << "Usage: mkdir <directory_name>" << std::endl;
        return;
    }
    fs->createDirectory(args[1]);
}

void Shell::handle_cd(const std::vector<std::string> &args)
{
    if (args.size() < 2)
    {
        fs->changeDirectory("/");
        return;
    }
    fs->changeDirectory(args[1]);
}

void Shell::handle_touch(const std::vector<std::string> &args)
{
    if (args.size() < 2)
    {
        std::cerr << "Usage: touch <file_name>" << std::endl;
        return;
    }
    fs->createFile(args[1]);
}

void Shell::handle_rm(const std::vector<std::string> &args)
{
    if (args.size() < 2)
    {
        std::cerr << "Usage: rm <file_name>" << std::endl;
        return;
    }
    fs->removeFile(args[1]);
}

void Shell::handle_rmdir(const std::vector<std::string> &args)
{
    if (args.size() < 2)
    {
        std::cerr << "Usage: rmdir <directory_name>" << std::endl;
        return;
    }
    fs->removeDirectory(args[1]);
}

void Shell::handle_echo(const std::string &command_line)
{
    // Simple parser for: echo "some content" > filename
    size_t first_quote = command_line.find('"');
    size_t second_quote = command_line.find('"', first_quote + 1);
    size_t redirect = command_line.find('>');

    if (first_quote == std::string::npos || second_quote == std::string::npos || redirect == std::string::npos)
    {
        std::cerr << "Usage: echo \"content\" > <filename>" << std::endl;
        return;
    }

    std::string content = command_line.substr(first_quote + 1, second_quote - first_quote - 1);
    std::string filename = command_line.substr(redirect + 1);
    // trim whitespace from filename
    filename.erase(0, filename.find_first_not_of(" \t\n\r"));
    filename.erase(filename.find_last_not_of(" \t\n\r") + 1);

    int inode_id = fs->openFile(filename);
    if (inode_id < 0)
    {
        inode_id = fs->createFile(filename);
        if (inode_id < 0)
        {
            std::cerr << "Error: Could not create file " << filename << std::endl;
            return;
        }
    }

    fs->writeFile(inode_id, content.c_str(), content.length(), 0); // 简化：从头开始写
    fs->closeFile(inode_id);
}

// 兼容旧签名：将分词结果拼回命令行，然后调用字符串版本
void Shell::handle_echo(const std::vector<std::string> &args)
{
    if (args.empty())
    {
        std::cerr << "Usage: echo \"content\" > <filename>" << std::endl;
        return;
    }
    // 简单拼接，保留空格
    std::string line;
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (i)
            line += ' ';
        line += args[i];
    }
    handle_echo(line);
}

void Shell::handle_cat(const std::vector<std::string> &args)
{
    if (args.size() < 2)
    {
        std::cerr << "Usage: cat <file_name>" << std::endl;
        return;
    }
    int inode_id = fs->openFile(args[1]);
    if (inode_id < 0)
    {
        std::cerr << "Error: File not found." << std::endl;
        return;
    }

    char buffer[4096]; // 一次最多读4KB
    int bytes_read = fs->readFile(inode_id, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';
        std::cout << buffer << std::endl;
    }
    fs->closeFile(inode_id);
}

void Shell::handle_format(const std::vector<std::string> &args)
{
    std::cout << "WARNING: This will erase all data on the disk. Are you sure? (y/n): ";
    std::string confirmation;
    std::getline(std::cin, confirmation);
    if (confirmation == "y" || confirmation == "Y")
    {
        fs->format();
    }
    else
    {
        std::cout << "Format aborted." << std::endl;
    }
}

void Shell::handle_help(const std::vector<std::string> &args)
{
    std::cout << "SimpleFS Shell - A simple file system simulation." << std::endl;
    std::cout << "Available commands:" << std::endl;
    std::cout << "  format              - Formats the virtual disk." << std::endl;
    std::cout << "  ls [path]           - Lists directory contents." << std::endl;
    std::cout << "  cd <path>           - Changes the current directory." << std::endl;
    std::cout << "  mkdir <dirname>     - Creates a new directory." << std::endl;
    std::cout << "  touch <filename>    - Creates a new empty file." << std::endl;
    std::cout << "  echo \"text\" > <file> - Writes text to a file." << std::endl;
    std::cout << "  cat <filename>      - Displays file content." << std::endl;
    std::cout << "  rm <filename>       - Removes a file (not fully implemented)." << std::endl;
    std::cout << "  rmdir <dirname>     - Removes an empty directory (not fully implemented)." << std::endl;
    std::cout << "  help                - Shows this help message." << std::endl;
    std::cout << "  exit                - Exits the shell." << std::endl;
}

void Shell::handle_exit(const std::vector<std::string> &args)
{
    std::cout << "Exiting SimpleFS shell." << std::endl;
    exit(0);
}

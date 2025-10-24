#include "disk_manager.h"
#include <iostream>
#include <cstring>

DiskManager::DiskManager()
{
    if (diskExists())
    {
        disk_file.open(DISK_PATH, std::ios::in | std::ios::out | std::ios::binary);
        if (!disk_file.is_open())
        {
            std::cerr << "Error: Could not open existing disk file." << std::endl;
        }
    }
}

DiskManager::~DiskManager()
{
    if (disk_file.is_open())
    {
        disk_file.close();
    }
}

bool DiskManager::diskExists()
{
    std::ifstream f(DISK_PATH.c_str());
    return f.good();
}

void DiskManager::createDisk()
{
    if (disk_file.is_open())
    {
        disk_file.close();
    }
    disk_file.open(DISK_PATH, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!disk_file.is_open())
    {
        std::cerr << "Error: Could not create disk file." << std::endl;
        return;
    }

    char *buffer = new char[BLOCK_SIZE];
    memset(buffer, 0, BLOCK_SIZE);
    for (int i = 0; i < DISK_BLOCKS; ++i)
    {
        disk_file.write(buffer, BLOCK_SIZE);
    }
    delete[] buffer;

    disk_file.close();
    // Reopen in r/w mode
    disk_file.open(DISK_PATH, std::ios::in | std::ios::out | std::ios::binary);
    if (!disk_file.is_open())
    {
        std::cerr << "Error: Could not reopen disk file after creation." << std::endl;
    }
}

bool DiskManager::readBlock(int block_id, char *buf)
{
    if (!disk_file.is_open() || block_id < 0 || block_id >= DISK_BLOCKS)
    {
        return false;
    }
    disk_file.seekg(block_id * BLOCK_SIZE, std::ios::beg);
    disk_file.read(buf, BLOCK_SIZE);
    return disk_file.good();
}

bool DiskManager::writeBlock(int block_id, const char *buf)
{
    if (!disk_file.is_open() || block_id < 0 || block_id >= DISK_BLOCKS)
    {
        return false;
    }
    disk_file.seekp(block_id * BLOCK_SIZE, std::ios::beg);
    disk_file.write(buf, BLOCK_SIZE);
    return disk_file.good();
}

#include "disk_manager.h"
#include <stdexcept>

const size_t PAGE_SIZE = 4096;

DiskManager::DiskManager(const std::string& fname)
    : filename(fname)
{
    file.open(filename, std::ios::in | std::ios::out | std::ios::binary);

    if (!file.is_open()) {
        file.clear();

        file.open(filename, std::ios::out | std::ios::binary);
        file.close();

        file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
    }

    if (!file.is_open()) {
        throw std::runtime_error("Cannot open database file");
    }
}

DiskManager::~DiskManager() {
    file.close();
}

void DiskManager::read_page(uint32_t page_id, char* data) {
    uint64_t offset = static_cast<uint64_t>(page_id) * PAGE_SIZE;

    file.seekg(offset, std::ios::beg);
    file.read(data, PAGE_SIZE);

    if (file.gcount() < PAGE_SIZE) {
        std::fill(data, data + PAGE_SIZE, 0);
    }
}

void DiskManager::write_page(uint32_t page_id, const char* data) {
    uint64_t offset = static_cast<uint64_t>(page_id) * PAGE_SIZE;

    file.seekp(offset, std::ios::beg);
    file.write(data, PAGE_SIZE);

    file.flush();
}

uint32_t DiskManager::get_page_count() {
    file.seekg(0, std::ios::end);
    uint64_t size = file.tellg();

    return static_cast<uint32_t>(size / PAGE_SIZE);
}
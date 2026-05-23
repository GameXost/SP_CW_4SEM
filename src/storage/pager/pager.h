#pragma once

#include <unordered_map>
#include <cstdint>
#include "../page/page.h"
#include "../disk/disk_manager.h"

class Pager {
private:
    std::unordered_map<uint32_t, Page*> pages;
    DiskManager disk;
    uint32_t next_page_id = 0;

public:
    Pager(const std::string& filename);

    Page* fetch_page(uint32_t page_id);
    Page* new_page();
    void flush_page(uint32_t page_id);
    uint32_t get_page_count();

    void flush_all();

    ~Pager();
};
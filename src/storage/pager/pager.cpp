#include "storage/pager/pager.h"

Pager::Pager(const std::string& filename)
    : disk(filename) {
        next_page_id = disk.get_page_count();
    }

Page* Pager::fetch_page(uint32_t page_id) {
    auto it = pages.find(page_id);
    if (it != pages.end()) {
        return it->second;
    }
    
    Page* page = new Page(page_id);

    if (page_id < disk.get_page_count()) {
        disk.read_page(page_id, page->data);
    } else {
        page->init(); // новая страница
    }

    pages[page_id] = page;
    return page;
}

Page* Pager::new_page() {
    uint32_t page_id = next_page_id++;

    Page* page = new Page(page_id);
    page->is_dirty = true;

    pages[page_id] = page;

    return page;
}

void Pager::flush_page(uint32_t page_id) {
    auto it = pages.find(page_id);
    if (it == pages.end()) return;

    Page* page = it->second;

    if (page->is_dirty) {
        disk.write_page(page_id, page->data);
        page->is_dirty = false;
    }
}

void Pager::flush_all() {
    for (auto& [id, page] : pages) {
        flush_page(id);
    }
}

uint32_t Pager::get_page_count() {
    return disk.get_page_count();
}

Pager::~Pager() {
    flush_all();
    for (auto& [id, page] : pages) {
        delete page;
    }
}
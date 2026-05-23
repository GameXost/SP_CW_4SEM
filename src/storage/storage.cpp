#include "../storage/storage.h"
#include "pager/pager.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

Storage::~Storage() = default;

std::string Storage::table_file(const std::string& db, const std::string& table) const {
    return root + db + "/" + table + ".db";
}

Pager& Storage::getPager(const std::string& db, const std::string& table) {
    std::string path = table_file(db, table);
    if (!pagers.count(path)) {
        pagers[path] = std::make_unique<Pager>(path);
    }
    return *pagers[path];
}

void Storage::createDatabase(const std::string& name) {
    fs::create_directory(root + name);
}

void Storage::dropDatabase(const std::string& name) {
    fs::remove_all(root + name);
}

void Storage::createTable(const std::string& db, const std::string& table) {
    std::string path = table_file(db, table);
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to create table file");
    }
}

void Storage::dropTable(const std::string& db, const std::string& table) {
    std::string path = table_file(db, table);
    fs::remove(path);
    pagers.erase(path);
}

RowId Storage::write(const std::string& db, const std::string& table, const std::vector<uint8_t>& bytes) {
    Pager& pager = getPager(db, table);

    uint32_t page_count = pager.get_page_count();
    Page* page = nullptr;
    uint32_t target_page;

    if (page_count == 0) {
        page = pager.new_page();
        target_page = page->page_id;
    } else {
        target_page = page_count - 1;
        page = pager.fetch_page(target_page);
    }

    uint16_t slot_id;

    if (!page->add_row(bytes.data(), static_cast<uint16_t>(bytes.size()), slot_id)) {
        page = pager.new_page();
        target_page = page->page_id;
        if (!page->add_row(bytes.data(), static_cast<uint16_t>(bytes.size()), slot_id)) {
            throw std::runtime_error("Row too large");
        }
    }

    pager.flush_page(target_page);
    return RowId{target_page, slot_id};
}

std::vector<std::pair<RowId, std::vector<uint8_t>>>
Storage::scan(const std::string& db, const std::string& table) {
    Pager& pager = getPager(db, table);

    std::vector<std::pair<RowId, std::vector<uint8_t>>> result;

    uint32_t page_count = pager.get_page_count();

    for (uint32_t p = 0; p < page_count; ++p) {
        Page* page = pager.fetch_page(p);
        uint16_t row_count = page->get_row_count();

        for (uint16_t s = 0; s < row_count; ++s) {
            uint16_t size;
            const uint8_t* data = page->get_row(s, size);

            if (!data) continue;

            std::vector<uint8_t> row(data, data + size);
            result.push_back({RowId{p, s}, row});    
        }
    }

    return result;
}

RowId Storage::update(const std::string& db, const std::string& table, const RowId& rid, const std::vector<uint8_t>& bytes) {
    remove(db, table, rid);
    return write(db, table, bytes);
}

void Storage::remove(const std::string& db, const std::string& table, const RowId& rid) {
    Pager& pager = getPager(db, table);
    Page* page = pager.fetch_page(rid.page_id);

    if (rid.slot_id >= page->get_row_count()) return;

    page->set_slot(rid.slot_id, Page::DELETED_SLOT);
    page->is_dirty = true;
    pager.flush_page(rid.page_id);
}
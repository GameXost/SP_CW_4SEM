#pragma once
#include <cstdint>
#include <unordered_map>
#include <stdexcept>
#include <fstream>
#include <filesystem>
#include <variant>
#include <string>
#include "index/btree_node.h"
#include "core/string_pool.h"

namespace detail {

inline void writeValue(std::ostream& out, const Value& v) {
    if (std::holds_alternative<std::monostate>(v)) {
        uint8_t tag = 0;
        out.write(reinterpret_cast<const char*>(&tag), 1);
    } else if (std::holds_alternative<int>(v)) {
        uint8_t tag = 1;
        out.write(reinterpret_cast<const char*>(&tag), 1);
        int32_t val = static_cast<int32_t>(std::get<int>(v));
        out.write(reinterpret_cast<const char*>(&val), sizeof(val));
    } else {
        uint8_t tag = 2;
        out.write(reinterpret_cast<const char*>(&tag), 1);
        const auto sv = std::get<std::string_view>(v);
        uint32_t len = static_cast<uint32_t>(sv.size());
        out.write(reinterpret_cast<const char*>(&len), sizeof(len));
        out.write(sv.data(), static_cast<std::streamsize>(len));
    }
}

inline Value readValue(std::istream& in) {
    uint8_t tag = 0;
    if (!in.read(reinterpret_cast<char*>(&tag), 1))
        throw std::runtime_error("DiskNodeStore: EOF reading Value tag");

    if (tag == 0) {
        return std::monostate{};
    } else if (tag == 1) {
        int32_t val = 0;
        if (!in.read(reinterpret_cast<char*>(&val), sizeof(val)))
            throw std::runtime_error("DiskNodeStore: EOF reading int");
        return static_cast<int>(val);
    } else if (tag == 2) {
        uint32_t len = 0;
        if (!in.read(reinterpret_cast<char*>(&len), sizeof(len)))
            throw std::runtime_error("DiskNodeStore: EOF reading string length");
        std::string s(len, '\0');
        if (!in.read(s.data(), static_cast<std::streamsize>(len)))
            throw std::runtime_error("DiskNodeStore: EOF reading string data");
        return StringPool::instance().intern(std::move(s));
    }

    throw std::runtime_error("DiskNodeStore: unknown Value tag: " + std::to_string(tag));
}

inline void writeNode(std::ostream& out, const BTreeNode& node) {
    uint8_t  is_leaf   = node.is_leaf ? 1 : 0;
    uint16_t key_count = node.key_count;

    out.write(reinterpret_cast<const char*>(&is_leaf),   sizeof(is_leaf));
    out.write(reinterpret_cast<const char*>(&key_count), sizeof(key_count));
    out.write(reinterpret_cast<const char*>(&node.parent_id), sizeof(node.parent_id));
    out.write(reinterpret_cast<const char*>(&node.next_leaf), sizeof(node.next_leaf));

    for (uint16_t i = 0; i < key_count; ++i)
        writeValue(out, node.keys[i]);

    if (node.is_leaf) {
        for (uint16_t i = 0; i < key_count; ++i) {
            out.write(reinterpret_cast<const char*>(&node.values[i].page_id), sizeof(uint32_t));
            out.write(reinterpret_cast<const char*>(&node.values[i].slot_id), sizeof(uint16_t));
        }
    } else {
        uint16_t num_children = static_cast<uint16_t>(key_count + 1);
        for (uint16_t i = 0; i < num_children; ++i)
            out.write(reinterpret_cast<const char*>(&node.children[i]), sizeof(uint32_t));
    }
}

inline BTreeNode readNode(std::istream& in) {
    BTreeNode node;

    uint8_t  is_leaf   = 0;
    uint16_t key_count = 0;

    if (!in.read(reinterpret_cast<char*>(&is_leaf),   sizeof(is_leaf)))
        throw std::runtime_error("DiskNodeStore: EOF reading is_leaf");
    if (!in.read(reinterpret_cast<char*>(&key_count), sizeof(key_count)))
        throw std::runtime_error("DiskNodeStore: EOF reading key_count");
    if (!in.read(reinterpret_cast<char*>(&node.parent_id), sizeof(node.parent_id)))
        throw std::runtime_error("DiskNodeStore: EOF reading parent_id");
    if (!in.read(reinterpret_cast<char*>(&node.next_leaf), sizeof(node.next_leaf)))
        throw std::runtime_error("DiskNodeStore: EOF reading next_leaf");

    node.is_leaf   = (is_leaf == 1);
    node.key_count = key_count;

    node.keys.resize(key_count);
    for (uint16_t i = 0; i < key_count; ++i)
        node.keys[i] = readValue(in);

    if (node.is_leaf) {
        node.values.resize(key_count);
        for (uint16_t i = 0; i < key_count; ++i) {
            uint32_t page_id = 0;
            uint16_t slot_id = 0;
            if (!in.read(reinterpret_cast<char*>(&page_id), sizeof(uint32_t)))
                throw std::runtime_error("DiskNodeStore: EOF reading RowId.page_id");
            if (!in.read(reinterpret_cast<char*>(&slot_id), sizeof(uint16_t)))
                throw std::runtime_error("DiskNodeStore: EOF reading RowId.slot_id");
            node.values[i] = RowId{page_id, slot_id};
        }
    } else {
        uint16_t num_children = static_cast<uint16_t>(key_count + 1);
        node.children.resize(num_children);
        for (uint16_t i = 0; i < num_children; ++i) {
            if (!in.read(reinterpret_cast<char*>(&node.children[i]), sizeof(uint32_t)))
                throw std::runtime_error("DiskNodeStore: EOF reading child");
        }
    }

    return node;
}

} // namespace detail


//  Абстрактный интерфейс хранилища нод
class NodeStore {
public:
    virtual ~NodeStore() = default;
    virtual BTreeNode load(uint32_t page_id) = 0;
    virtual void      save(uint32_t page_id, const BTreeNode& node) = 0;
    virtual uint32_t  alloc() = 0;
    virtual void      free(uint32_t page_id) = 0;
    virtual uint32_t  getRoot() = 0;
    virtual void      setRoot(uint32_t page_id) = 0;
};


//  In-memory заглушка - только для тестов, данные не переживают рестарт
class MemNodeStore : public NodeStore {
public:
    BTreeNode load(uint32_t page_id) override {
        auto it = _pages.find(page_id);
        if (it == _pages.end())
            throw std::runtime_error("MemNodeStore: page not found");
        return it->second;
    }
    void save(uint32_t page_id, const BTreeNode& node) override {
        _pages[page_id] = node;
    }
    uint32_t alloc() override {
        uint32_t id = _next_id++;
        _pages[id] = BTreeNode{};
        return id;
    }
    void free(uint32_t page_id) override { _pages.erase(page_id); }
    uint32_t getRoot() override { return _root; }
    void setRoot(uint32_t page_id) override { _root = page_id; }

private:
    std::unordered_map<uint32_t, BTreeNode> _pages;
    uint32_t _next_id = 0;
    uint32_t _root    = NULL_PAGE;
};


class DiskNodeStore : public NodeStore {
public:
    explicit DiskNodeStore(const std::string& path) : _path(path) {
        _load_all();
    }

    ~DiskNodeStore() override {
        try { _flush(); } catch (...) {}
    }

    void flush() { _flush(); }

    BTreeNode load(uint32_t page_id) override {
        auto it = _cache.find(page_id);
        if (it == _cache.end())
            throw std::runtime_error(
                "DiskNodeStore: page not found: " + std::to_string(page_id));
        return it->second;
    }

    void save(uint32_t page_id, const BTreeNode& node) override {
        _cache[page_id] = node;
        _dirty = true;
    }

    uint32_t alloc() override {
        uint32_t id = _next_id++;
        _cache[id] = BTreeNode{};
        _dirty = true;
        return id;
    }

    void free(uint32_t page_id) override {
        _cache.erase(page_id);
        _dirty = true;
    }

    uint32_t getRoot() override { return _root; }

    void setRoot(uint32_t page_id) override {
        _root  = page_id;
        _dirty = true;
    }

private:
    std::string _path;
    std::unordered_map<uint32_t, BTreeNode> _cache;
    uint32_t _root    = NULL_PAGE;
    uint32_t _next_id = 0;
    bool     _dirty   = false;

    void _load_all() {
        std::ifstream f(_path, std::ios::binary);
        if (!f) return; // файл не существует — первый запуск, пустой индекс

        if (!f.read(reinterpret_cast<char*>(&_root),    sizeof(_root)))    return;
        if (!f.read(reinterpret_cast<char*>(&_next_id), sizeof(_next_id))) return;

        uint32_t page_id = 0;
        while (f.read(reinterpret_cast<char*>(&page_id), sizeof(page_id))) {
            _cache[page_id] = detail::readNode(f);
            if (f.fail())
                throw std::runtime_error(
                    "DiskNodeStore: corrupted index file: " + _path);
        }
    }

    void _flush() {
        if (!_dirty) return;

        std::ofstream f(_path, std::ios::binary | std::ios::trunc);
        if (!f)
            throw std::runtime_error(
                "DiskNodeStore: cannot open for write: " + _path);

        f.write(reinterpret_cast<const char*>(&_root),    sizeof(_root));
        f.write(reinterpret_cast<const char*>(&_next_id), sizeof(_next_id));

        for (const auto& [page_id, node] : _cache) {
            f.write(reinterpret_cast<const char*>(&page_id), sizeof(page_id));
            detail::writeNode(f, node);
        }

        if (!f)
            throw std::runtime_error(
                "DiskNodeStore: write failed: " + _path);

        _dirty = false;
    }
};
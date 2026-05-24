#pragma once
#include <cstdint>
#include <unordered_map>
#include <stdexcept>
#include "index/btree_node.h"

// абстрактный интерфейс — storage-чел реализует DiskNodeStore поверх него
class NodeStore {
public:
    virtual ~NodeStore() = default;
    virtual BTreeNode load(uint32_t page_id) = 0;
    virtual void save(uint32_t page_id, const BTreeNode& node) = 0;
    virtual uint32_t alloc() = 0;
    virtual void free(uint32_t page_id) = 0;
    virtual uint32_t getRoot() = 0;
    virtual void setRoot(uint32_t page_id) = 0;
};

// заглушка в памяти для тестов работает без диска
// когда будет DiskNodeStore по идее просто поменять в IndexManager
class MemNodeStore : public NodeStore {
public:
    BTreeNode load(uint32_t page_id) override {
        auto it = _pages.find(page_id);
        if (it == _pages.end()) throw std::runtime_error("MemNodeStore: page not found");
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
    void free(uint32_t page_id) override {
        _pages.erase(page_id);
    }
    uint32_t getRoot() override { return _root; }
    void setRoot(uint32_t page_id) override { _root = page_id; }

private:
    std::unordered_map<uint32_t, BTreeNode> _pages;
    uint32_t _next_id = 0;
    uint32_t _root = NULL_PAGE;
};
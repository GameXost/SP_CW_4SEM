#pragma once
#include <optional>
#include <vector>
#include <cstdint>
#include "core/types.h"
#include "index/btree_node.h"
#include "index/node_store.h"

// докинуты доп функции и структура. мб фикс, если надо будет
class BTree {
public:
    explicit BTree(NodeStore& store);

    // в хелперах делать
    std::optional<RowId> find(const Value& key);

    // в хелперах делать
    // возвращает все RowId где key в [lo, hi)
    std::vector<RowId> scanRange(const Value& lo, const Value& hi);

    // вставка (key, rid)
    // бросает runtime_error при дубле - INDEXED = unique
    // госпожа В. 
    void insert(const Value& key, RowId rid);
    void split(const Value& key, RowId rid);

    // удаление по ключу
    // возвращает false если ключ не найден
    // господин Др. 
    bool erase(const Value& key);
    bool merge(const Value& key);

private:
    NodeStore& _store;

    // в хелперах делать по идее

    uint32_t findLeaf(const Value& key);

    static int compareValues(const Value& a, const Value& b);

    static int findChildIndex(const BTreeNode& parent, uint32_t child_page_id);

    // госпожа В. isert/split
    void insertIntoLeaf(uint32_t leaf_id, const Value& key, RowId rid);

    void insertIntoInternal(uint32_t node_id, const Value& key, uint32_t right_id);

    void redistributeOrSplitLeaf(uint32_t leaf_id);

    void redistributeOrSplitInternal(uint32_t node_id);

    // господин Др. merge/erase
    bool eraseFromLeaf(uint32_t leaf_id, const Value& key);

    void fixUnderflow(uint32_t node_id);

    bool tryBorrowLeft(uint32_t node_id, uint32_t parent_id, uint16_t child_idx);

    bool tryBorrowRight(uint32_t node_id, uint32_t parent_id, uint16_t child_idx);

    void mergeWithLeft(uint32_t node_id, uint32_t parent_id, uint16_t child_idx);

    void mergeWithRight(uint32_t node_id, uint32_t parent_id, uint16_t child_idx);
};
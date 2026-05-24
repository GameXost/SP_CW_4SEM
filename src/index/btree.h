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

    std::optional<RowId> find(const Value& key);
    std::vector<RowId> scanRange(const Value& lo, const Value& hi);

    void insert(const Value& key, RowId rid);

    bool erase(const Value& key);

    static int compareValues(const Value& a, const Value& b);
private:
    NodeStore& _store;

    uint32_t findLeaf(const Value& key);

    static int findChildIndex(const BTreeNode& parent, uint32_t child_page_id);

    static void leafInsertAt(BTreeNode& node, uint16_t pos, const Value& key, RowId rid);

    static void internalInsertAt(BTreeNode& node, uint16_t pos, const Value& key, uint32_t right_id);

    static uint16_t lowerBound(const std::vector<Value>& keys, uint16_t count, const Value& key);

    void redistributeOrSplitLeaf(uint32_t leaf_id);

    void redistributeOrSplitInternal(uint32_t node_id);

    bool eraseFromLeaf(uint32_t leaf_id, const Value& key);

    void fixUnderflow(uint32_t node_id);

    bool tryBorrowLeft(uint32_t node_id, uint32_t parent_id, uint16_t child_idx);

    bool tryBorrowRight(uint32_t node_id, uint32_t parent_id, uint16_t child_idx);

    void mergeWithLeft(uint32_t node_id, uint32_t parent_id, uint16_t child_idx);

    void mergeWithRight(uint32_t node_id, uint32_t parent_id, uint16_t child_idx);
};
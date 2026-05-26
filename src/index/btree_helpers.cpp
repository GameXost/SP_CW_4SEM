#include "index/btree.h"
#include <stdexcept>

// создаёт пустой корень-лист если дерево только что инициализировано
BTree::BTree(NodeStore& store) : _store(store) {
    if (_store.getRoot() == NULL_PAGE) {
        uint32_t root_id = _store.alloc();
        BTreeNode root;
        root.is_leaf = true;
        root.parent_id = NULL_PAGE;
        _store.save(root_id, root);
        _store.setRoot(root_id);
    }
}

int BTree::compareValues(const Value& a, const Value& b) {
    // оба int
    if (std::holds_alternative<int>(a) && std::holds_alternative<int>(b)) {
        int ia = std::get<int>(a);
        int ib = std::get<int>(b);
        return (ia < ib) ? -1 : (ia > ib) ? 1 : 0;
    }
    // оба string
    if (std::holds_alternative<std::string_view>(a) && std::holds_alternative<std::string_view>(b)) {
        const auto sa = std::get<std::string_view>(a);
        const auto sb = std::get<std::string_view>(b);
        if (sa < sb) return -1;
        if (sa > sb) return 1;
        return 0;
    }
    // оба NULL — равны
    if (std::holds_alternative<std::monostate>(a) && std::holds_alternative<std::monostate>(b)) {
        return 0;
    }
    // разные типы: monostate < int < string
    auto rank = [](const Value& v) -> int {
        if (std::holds_alternative<std::monostate>(v)) return 0;
        if (std::holds_alternative<int>(v)) return 1;
        return 2;
    };
    return rank(a) - rank(b);
}


// линейный поиск child_page_id в parent.children бросает исключение, если не нашёл
int BTree::findChildIndex(const BTreeNode& parent, uint32_t child_page_id) {
    for (int i = 0; i <= static_cast<int>(parent.key_count); ++i) {
        if (parent.children[i] == child_page_id) return i;
    }
    throw std::runtime_error("BTree::findChildIndex: child not found in parent");
}


uint32_t BTree::findLeaf(const Value& key) {
    uint32_t cur = _store.getRoot();
    if (cur == NULL_PAGE) return NULL_PAGE;
    while (true) {
        BTreeNode node = _store.load(cur);
        if (node.is_leaf) return cur;
        uint32_t next = node.children[node.key_count];
        for (uint16_t i = 0; i < node.key_count; ++i) {
            if (compareValues(key, node.keys[i]) < 0) {
                next = node.children[i];
                break;
            }
        }
        cur = next;
    }
}

std::optional<RowId> BTree::find(const Value& key) {
    uint32_t leaf_id = findLeaf(key);
    if (leaf_id == NULL_PAGE) return std::nullopt;
    BTreeNode leaf = _store.load(leaf_id);
    for (uint16_t i = 0; i < leaf.key_count; ++i) {
        int cmp = compareValues(key, leaf.keys[i]);
        if (cmp == 0) return leaf.values[i];
        if (cmp < 0) break;
    }
    return std::nullopt;
}


std::vector<RowId> BTree::scanRange(const Value& lo, const Value& hi) {
    std::vector<RowId> result;
    uint32_t leaf_id = findLeaf(lo);
    if (leaf_id == NULL_PAGE) return result;
    while (leaf_id != NULL_PAGE) {
        BTreeNode leaf = _store.load(leaf_id);
        bool past_hi = false;
        for (uint16_t i = 0; i < leaf.key_count; ++i) {
            if (compareValues(leaf.keys[i], hi) >= 0) { past_hi = true; break; }
            if (compareValues(leaf.keys[i], lo) >= 0) result.push_back(leaf.values[i]);
        }
        if (past_hi) break;
        leaf_id = leaf.next_leaf;
    }
    return result;
}
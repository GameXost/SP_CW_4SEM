#pragma once
#include <vector>
#include <cstdint>
#include "core/types.h"

static constexpr uint32_t NULL_PAGE = 0xFFFFFFFF;

static constexpr uint16_t BTREE_MAX_KEYS = 100;

static constexpr uint16_t BTREE_MIN_KEYS = (BTREE_MAX_KEYS * 2 + 2) / 3;

struct BTreeNode {
    bool is_leaf = true;
    uint16_t key_count = 0;

    // NULL_PAGE означает нет родителя (узел — корень)
    uint32_t parent_id = NULL_PAGE;

    // linked list листьев - нужен для scanRange. у internal узлов не используется
    uint32_t next_leaf = NULL_PAGE;

    // key_count ключей, отсортированных по возрастанию
    std::vector<Value> keys;

    // только у internal: key_count + 1 потомков
    // children[i] содержит ключи < keys[i]
    // children[key_count] содержит ключи >= keys[key_count - 1]
    std::vector<uint32_t> children;

    // только у листьев: key_count записей
    // values[i] - RowId строки с ключом keys[i]
    std::vector<RowId> values;
};
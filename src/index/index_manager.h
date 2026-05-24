#ifndef CW_INDEX_MANAGER_H
#define CW_INDEX_MANAGER_H

#include <string>
#include <unordered_map>
#include <memory>
#include <optional>
#include <vector>
#include "core/types.h"
#include "index/btree.h"
#include "index/node_store.h"

class IndexManager {
public:
    void create(const std::string& db, const std::string& table, const std::string& column) {
        (void)db; (void)table; (void)column;
        // TODO: B*-tree + connect (db, table, column)
    }

    void drop(const std::string& db, const std::string& table, const std::string& column) {
        (void)db; (void)table; (void)column;
        // TODO: delete B*-tree and file
    }

    // unique checker (TODO after B*-tree)
    void insert(const std::string& db, const std::string& table, const std::string& column,
                const Value& key, RowId rid) {
        (void)db; (void)table; (void)column; (void)key; (void)rid;
        // TODO: tree.insert(key, rid); if key -> throw "Unique violation"
    }

    void erase(const std::string& db, const std::string& table, const std::string& column,
               const Value& key) {
        (void)db; (void)table; (void)column; (void)key;
        // TODO: tree.erase(key)
    }

    std::optional<RowId> find(const std::string& db, const std::string& table, const std::string& column, const Value& key);
    std::vector<RowId> scanRange(const std::string& db, const std::string& table, const std::string& column, const Value& lo, const Value& hi);

private:
    struct Entry {
        std::unique_ptr<NodeStore> store;
        std::unique_ptr<BTree> tree;
    };
    std::unordered_map<std::string, Entry> _indexes;
    static std::string makeKey(const std::string& db, const std::string& table,
                               const std::string& column);
};
#endif
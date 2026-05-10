#ifndef CW_INDEX_MANAGER_H
#define CW_INDEX_MANAGER_H

#include <string>
#include "core/types.h"

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
};

#endif
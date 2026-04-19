#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include "core/types.h"
#include "parser/ast.h"

// Схема таблицы — список колонок
struct TableSchema {
    std::string            db;
    std::string            table;
    std::vector<ColumnDef> columns;

    // удобный поиск индекса колонки по имени
    int indexOf(const std::string& name) const {
        for (int i = 0; i < (int)columns.size(); ++i)
            if (columns[i].name == name) return i;
        return -1;
    }
};

// Каталог: хранит метаданные о базах и таблицах
class Catalog {
public:
    void createDatabase(const std::string& db) {
        if (_dbs.count(db)) throw std::runtime_error("Database already exists: " + db);
        _dbs[db];  // создаём пустую запись
    }

    void dropDatabase(const std::string& db) {
        requireDb(db);
        _dbs.erase(db);
    }

    bool databaseExists(const std::string& db) const {
        return _dbs.count(db) > 0;
    }

    void createTable(const std::string& db, const std::string& table,
                     const std::vector<ColumnDef>& columns)
    {
        requireDb(db);
        auto& tables = _dbs.at(db);
        if (tables.count(table)) throw std::runtime_error("Table already exists: " + table);
        tables[table] = TableSchema{db, table, columns};
    }

    void dropTable(const std::string& db, const std::string& table) {
        requireDb(db);
        requireTable(db, table);
        _dbs.at(db).erase(table);
    }

    bool tableExists(const std::string& db, const std::string& table) const {
        return _dbs.count(db) && _dbs.at(db).count(table);
    }

    const TableSchema& getSchema(const std::string& db, const std::string& table) const {
        requireDb(db);
        requireTable(db, table);
        return _dbs.at(db).at(table);
    }

private:
    // db_name → (table_name → schema)
    std::unordered_map<std::string,
        std::unordered_map<std::string, TableSchema>> _dbs;

    void requireDb(const std::string& db) const {
        if (!_dbs.count(db)) throw std::runtime_error("Database not found: " + db);
    }
    void requireTable(const std::string& db, const std::string& table) const {
        if (!_dbs.count(db) || !_dbs.at(db).count(table))
            throw std::runtime_error("Table not found: " + db + "." + table);
    }
};
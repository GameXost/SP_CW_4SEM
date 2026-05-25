#include "index/index_manager.h"
#include <filesystem>

std::string IndexManager::makeKey(const std::string& db, const std::string& table, const std::string& column) {
    return db + "." + table + "." + column;
}

void IndexManager::create(const std::string& db, const std::string& table, const std::string& column) {
    std::string key = makeKey(db, table, column);
    if (_indexes.count(key)) return;

    std::unique_ptr<NodeStore> store;
    if (_indexDir.empty()) {
        // fallback для тестов
        store = std::make_unique<MemNodeStore>();
    } else {
        // формируем путь: data/indexes/mydb.users.id.idx
        std::filesystem::create_directories(_indexDir);
        std::string path = _indexDir + "/" + key + ".idx";
        store = std::make_unique<DiskNodeStore>(path);
    }

    auto tree = std::make_unique<BTree>(*store);
    _indexes[key] = Entry{std::move(store), std::move(tree)};
}

void IndexManager::drop(const std::string& db, const std::string& table, const std::string& column) {
    _indexes.erase(makeKey(db, table, column));
}

void IndexManager::insert(const std::string& db, const std::string& table, const std::string& column, const Value& key, RowId rid) {
    auto it = _indexes.find(makeKey(db, table, column));
    if (it == _indexes.end()) return;
    it->second.tree->insert(key, rid);
}

void IndexManager::erase(const std::string& db, const std::string& table, const std::string& column, const Value& key) {
    auto it = _indexes.find(makeKey(db, table, column));
    if (it == _indexes.end()) return;
    it->second.tree->erase(key);
}

std::optional<RowId> IndexManager::find(const std::string& db, const std::string& table, const std::string& column, const Value& key) {
    auto it = _indexes.find(makeKey(db, table, column));
    if (it == _indexes.end()) return std::nullopt;
    return it->second.tree->find(key);
}

std::vector<RowId> IndexManager::scanRange(const std::string& db, const std::string& table, const std::string& column, const Value& lo, const Value& hi) {
    auto it = _indexes.find(makeKey(db, table, column));
    if (it == _indexes.end()) return {};
    return it->second.tree->scanRange(lo, hi);
}
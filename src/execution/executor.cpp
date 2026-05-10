#include "execution/executor.h"
#include "execution/serializer.h"
#include <regex>
#include <stdexcept>
#include <unordered_map>

Executor::Executor(Catalog& catalog, Storage& storage) : _catalog(catalog), _storage(storage) {}

ExecuteResult Executor::execute(ASTNode& node) {
    _result = {};
    try {
        node.accept(*this);
    } catch (const std::exception& e) {
        _result.ok = false;
        _result.message = e.what();
    }
    return _result;
}

std::pair<std::string, std::string> Executor::resolve(const TableReference& ref) const {
    std::string db = ref.db.value_or(_current_db);
    if (db.empty()) throw std::runtime_error("No database selected. Use USE <db>.");
    return {db, ref.table};
}

// DDL db
void Executor::visit(CreateDatabaseStmt& s) {
    _catalog.createDatabase(s.name);
    _storage.createDatabase(s.name);
    _result.message = "Database '" + s.name + "' created.";
}

void Executor::visit(DropDatabaseStmt& s) {
    _catalog.dropDatabase(s.name);
    _storage.dropDatabase(s.name);
    _result.message = "Database '" + s.name + "' dropped.";
}

void Executor::visit(UseStmt& s) {
    if (!_catalog.databaseExists(s.name))
        throw std::runtime_error("Unknown database: " + s.name);
    _current_db = s.name;
    _result.message = "Database changed to '" + s.name + "'.";
}

// DDL table
void Executor::visit(CreateTableStmt& s) {
    auto [db, table] = resolve(s.table);
    _catalog.createTable(db, table, s.columns);
    _storage.createTable(db, table);

    for (const auto& col : s.columns) {
        if (col.constraint == Constraint::INDEXED) {
            _index.create(db, table, col.name);
        }
    }

    _result.message = "Table '" + db + "." + table + "' created.";
}

void Executor::visit(DropTableStmt& s) {
    auto [db, table] = resolve(s.table);
    _catalog.dropTable(db, table);
    _storage.dropTable(db, table);

    for (const auto& col : schema.columns) {
        if (col.constraint == Constraint::INDEXED) {
            _index.drop(db, table, col.name);
        }
    }

    _result.message = "Table '" + db + "." + table + "' dropped.";
}

// INSERT
void Executor::visit(InsertStmt& s) {
    auto [db, table] = resolve(s.table);
    const auto& schema = _catalog.getSchema(db, table);
    int inserted = 0;

    for (const auto& src_row : s.rows) {
        if (src_row.size() != s.columns.size()) {
            throw std::runtime_error("INSERT: column count mismatch");
        }

        std::vector<Value> full_row(schema.columns.size(), std::monostate{});

        for (size_t i = 0; i < s.columns.size(); ++i) {
            int idx = schema.indexOf(s.columns[i]);
            if (idx < 0) throw std::runtime_error("Unknown column: " + s.columns[i]);
            full_row[idx] = src_row[i];
        }

        for (size_t i = 0; i < schema.columns.size(); ++i) {
            const auto& col = schema.columns[i];
            bool is_null = std::holds_alternative<std::monostate>(full_row[i]);
            if (is_null && col.constraint != Constraint::NONE) {
                throw std::runtime_error("NOT_NULL/INDEXED violation: " + col.name);
            }
        }

        _storage.write(db, table, Serializer::encodeRow(full_row));
        ++inserted;
    }

    _result.message = "Inserted " + std::to_string(inserted) + " row(s).";
}

// UPDATE
void Executor::visit(UpdateStmt& s) {
    auto [db, table] = resolve(s.table);
    const auto& schema = _catalog.getSchema(db, table);

    for (const auto& [col, val] : s.assignments) {
        if (schema.indexOf(col) < 0) throw std::runtime_error("Unknown column: " + col);
    }

    auto records = _storage.scan(db, table);
    int updated = 0;

    for (auto& [rid, bytes] : records) {
        auto row = Serializer::decodeRow(bytes);
        auto row_map = makeRowMap(row, schema);
        if (!matchRow(s.where.get(), row_map)) continue;

        for (const auto& [col, val] : s.assignments) {
            row[schema.indexOf(col)] = val;
        }

        _storage.update(db, table, rid, Serializer::encodeRow(row));
        ++updated;
    }

    _result.message = "Updated " + std::to_string(updated) + " row(s).";
}

// DELETE
void Executor::visit(DeleteStmt& s) {
    auto [db, table] = resolve(s.table);
    const auto& schema = _catalog.getSchema(db, table);
    auto records = _storage.scan(db, table);
    int deleted = 0;

    for (auto& [rid, bytes] : records) {
        auto row_map = makeRowMap(Serializer::decodeRow(bytes), schema);
        if (!matchRow(s.where.get(), row_map)) continue;
        _storage.remove(db, table, rid);
        ++deleted;
    }

    _result.message = "Deleted " + std::to_string(deleted) + " row(s).";
}

// SELECT
void Executor::visit(SelectStmt& s) {
    auto [db, table] = resolve(s.table);
    const auto& schema = _catalog.getSchema(db, table);

    std::vector<std::string> proj_cols;
    if (s.columns.empty()) {
        for (const auto& c : schema.columns) proj_cols.push_back(c.name);
    } else {
        for (const auto& sc : s.columns) proj_cols.push_back(sc.name);
    }

    std::unordered_map<std::string, std::string> aliases;
    for (const auto& sc : s.columns) {
        aliases[sc.name] = sc.alias.value_or(sc.name);
    }

    auto records = _storage.scan(db, table);
    nlohmann::json result = nlohmann::json::array();

    for (auto& [rid, bytes] : records) {
        auto row_map = makeRowMap(Serializer::decodeRow(bytes), schema);
        if (!matchRow(s.where.get(), row_map)) continue;

        nlohmann::json obj;
        for (const auto& col : proj_cols) {
            if (!row_map.count(col)) throw std::runtime_error("Unknown column in SELECT: " + col);
            std::string out_name = aliases.count(col) ? aliases.at(col) : col;
            obj[out_name] = valueToJson(row_map.at(col));
        }
        result.push_back(std::move(obj));
    }

    _result.message = "OK";
    _result.data = std::move(result);
}

// helpers
std::unordered_map<std::string, Value> Executor::makeRowMap(const std::vector<Value>& row, const TableSchema& schema) const {
    std::unordered_map<std::string, Value> m;
    for (size_t i = 0; i < schema.columns.size() && i < row.size(); ++i) {
        m[schema.columns[i].name] = row[i];
    }
    return m;
}

nlohmann::json Executor::valueToJson(const Value& v) {
    if (std::holds_alternative<std::monostate>(v)) return nullptr;
    if (std::holds_alternative<int>(v)) return std::get<int>(v);
    return std::get<std::string>(v);
}

bool Executor::matchRow(const ExprNode* where, const std::unordered_map<std::string, Value>& row) const {
    if (!where) return true;
    Value result = evalExpr(where, row);
    if (std::holds_alternative<int>(result)) return std::get<int>(result) != 0;
    return false;
}

// WHERE recursive eval
Value Executor::evalExpr(const ExprNode* expr, const std::unordered_map<std::string, Value>& row) const {
    if (const auto* lit = dynamic_cast<const LiteralExpr*>(expr)) {
        return lit->value;
    }

    if (const auto* col = dynamic_cast<const ColumnRefExpr*>(expr)) {
        auto it = row.find(col->name);
        if (it == row.end()) throw std::runtime_error("Unknown column in WHERE: " + col->name);
        return it->second;
    }

    if (const auto* bin = dynamic_cast<const BinaryExpr*>(expr)) {
        Value lv = evalExpr(bin->left.get(), row);
        Value rv = evalExpr(bin->right.get(), row);

        if (std::holds_alternative<std::monostate>(lv) || std::holds_alternative<std::monostate>(rv))
            return 0;

        auto cmp = [&]() -> int {
            if (std::holds_alternative<int>(lv) && std::holds_alternative<int>(rv))
                return std::get<int>(lv) - std::get<int>(rv);
            if (std::holds_alternative<std::string>(lv) && std::holds_alternative<std::string>(rv))
                return std::get<std::string>(lv).compare(std::get<std::string>(rv));
            throw std::runtime_error("Type mismatch in WHERE comparison");
        };

        switch (bin->oper) {
            case BinaryOper::EQ: return (cmp() == 0) ? 1 : 0;
            case BinaryOper::NEQ: return (cmp() != 0) ? 1 : 0;
            case BinaryOper::LT: return (cmp() < 0) ? 1 : 0;
            case BinaryOper::GT: return (cmp() > 0) ? 1 : 0;
            case BinaryOper::LTE: return (cmp() <= 0) ? 1 : 0;
            case BinaryOper::GTE: return (cmp() >= 0) ? 1 : 0;
            default: throw std::runtime_error("Unknown binary operator");
        }
    }

    if (const auto* bet = dynamic_cast<const BetweenExpr*>(expr)) {
        Value val = evalExpr(bet->value.get(), row);
        Value lo = evalExpr(bet->low.get(), row);
        Value hi = evalExpr(bet->high.get(), row);

        if (std::holds_alternative<int>(val)) {
            int v = std::get<int>(val);
            int l = std::get<int>(lo);
            int h = std::get<int>(hi);
            return (v >= l && v < h) ? 1 : 0;
        }
        if (std::holds_alternative<std::string>(val)) {
            const auto& v = std::get<std::string>(val);
            const auto& l = std::get<std::string>(lo);
            const auto& h = std::get<std::string>(hi);
            return (v >= l && v < h) ? 1 : 0;
        }
        return 0;
    }

    if (const auto* like = dynamic_cast<const LikeExpr*>(expr)) {
        Value val = evalExpr(like->value.get(), row);
        if (!std::holds_alternative<std::string>(val)) return 0;
        try {
            std::regex re(like->pattern);
            return std::regex_match(std::get<std::string>(val), re) ? 1 : 0;
        } catch (const std::regex_error&) {
            throw std::runtime_error("Invalid LIKE pattern: " + like->pattern);
        }
    }

    if (const auto* log = dynamic_cast<const LogicalExpr*>(expr)) {
        Value lv = evalExpr(log->left.get(), row);
        bool lb = std::holds_alternative<int>(lv) && std::get<int>(lv) != 0;

        if (log->oper == LogicalOper::AND) {
            if (!lb) return 0;
            Value rv = evalExpr(log->right.get(), row);
            return (std::holds_alternative<int>(rv) && std::get<int>(rv) != 0) ? 1 : 0;
        } else {
            if (lb) return 1;
            Value rv = evalExpr(log->right.get(), row);
            return (std::holds_alternative<int>(rv) && std::get<int>(rv) != 0) ? 1 : 0;
        }
    }

    throw std::runtime_error("Executor: unknown ExprNode type in WHERE");
}
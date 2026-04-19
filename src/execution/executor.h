#pragma once
#include "parser/ast.h"
#include "core/result.h"
#include "catalog/catalog.h"
#include "storage/storage.h"
// #include "include/nlohmann/json.hpp"


class Executor : public Visitor {
public:
    Executor(Catalog& catalog, Storage& storage);

    ExecuteResult execute(ASTNode& node);

    // visitor interface
    void visit(CreateDatabaseStmt&) override;
    void visit(DropDatabaseStmt&)   override;
    void visit(UseStmt&)            override;
    void visit(CreateTableStmt&)    override;
    void visit(DropTableStmt&)      override;
    void visit(InsertStmt&)         override;
    void visit(UpdateStmt&)         override;
    void visit(DeleteStmt&)         override;
    void visit(SelectStmt&)         override;

private:
    Catalog&      _catalog;
    Storage&      _storage;
    std::string   _current_db;   // установлено командой USE
    ExecuteResult _result;       // заполняется внутри visit-методов

    std::pair<std::string, std::string> resolve(const TableReference& ref) const;

    // do WHERE expr for row
    Value evalExpr(const ExprNode* expr, const std::unordered_map<std::string, Value>& row) const;

    // true if WHERE // null -> all match 
    bool matchRow(const ExprNode* where, const std::unordered_map<std::string, Value>& row) const;

    // do map {col_name -> value} from vector
    std::unordered_map<std::string, Value>
        makeRowMap(const std::vector<Value>& row, const TableSchema& schema) const;

    // value to json nlohmann
    static nlohmann::json valueToJson(const Value& v);
};
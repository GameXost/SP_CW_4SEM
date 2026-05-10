//
// Created by GameXost on 19.04.2026.
//

#include "parser/parser.h"
#include "core/exceptions.h"
#include <charconv>

Parser::Parser(std::vector<Token> tokens)
    : _vector(std::move(tokens)), _position(0) {}

// возвращает текущий токен без сдвига
Token& Parser::current() {
    if (_position >= _vector.size())
        throw SyntaxError("неожиданный конец ввода");
    return _vector[_position];
}

// возвращает токен со смещением, позицию не двигает
Token& Parser::peek(size_t offset) {
    size_t new_pos = _position + offset;
    if (new_pos >= _vector.size()) return _vector.back();
    return _vector[new_pos];
}

// проверяет тип текущего токена без сдвига
bool Parser::check(TokenType type) {
    return current().type == type;
}

// проверяет тип, сдвигается вперёд, при несовпадении - ошибка
Token Parser::consume(TokenType expected) {
    if (!check(expected)) {
        throw SyntaxError(
            "Строка " + std::to_string(current().line) +
            ": ожидался токен " + std::to_string(static_cast<int>(expected)) +
            ", найден '" + current().value + "'"
        );
    }
    Token token = current();
    _position++;
    return token;
}

// мягкая проверка - если совпало, сдвигается и возвращает true
bool Parser::match(TokenType type) {
    if (!check(type)) return false;
    _position++;
    return true;
}

// точка входа - парсит одну команду и ожидает ;
ASTNodePtr Parser::parse() {
    ASTNodePtr node = parseStatement();
    consume(TokenType::SYM_SEMICOLON);
    return node;
}

// распределяет по типу команды
ASTNodePtr Parser::parseStatement() {
    switch (current().type) {
        case TokenType::K_CREATE: return parseCreate();
        case TokenType::K_DROP:   return parseDrop();
        case TokenType::K_USE:    return parseUse();
        case TokenType::K_INSERT: return parseInsert();
        case TokenType::K_UPDATE: return parseUpdate();
        case TokenType::K_DELETE: return parseDelete();
        case TokenType::K_SELECT: return parseSelect();
        default:
            throw SyntaxError(
                "Строка " + std::to_string(current().line) +
                ": неизвестная команда '" + current().value + "'"
            );
    }
}

// проваливается в -> CREATE DATABASE или CREATE TABLE
ASTNodePtr Parser::parseCreate() {
    consume(TokenType::K_CREATE);
    if (check(TokenType::K_DATABASE)) return parseCreateDatabase();
    if (check(TokenType::K_TABLE))    return parseCreateTable();
    throw SyntaxError(
        "Строка " + std::to_string(current().line) +
        ": ожидалось DATABASE или TABLE после CREATE"
    );
}

// проваливается в -> DROP DATABASE или DROP TABLE
ASTNodePtr Parser::parseDrop() {
    consume(TokenType::K_DROP);
    if (check(TokenType::K_DATABASE)) return parseDropDatabase();
    if (check(TokenType::K_TABLE))    return parseDropTable();
    throw SyntaxError(
        "Строка " + std::to_string(current().line) +
        ": ожидалось DATABASE или TABLE после DROP"
    );
}

ASTNodePtr Parser::parseCreateDatabase() {
    consume(TokenType::K_DATABASE);
    auto node  = std::make_unique<CreateDatabaseStmt>();
    node->name = consume(TokenType::IDENT).value;
    return node;
}

ASTNodePtr Parser::parseDropDatabase() {
    consume(TokenType::K_DATABASE);
    auto node  = std::make_unique<DropDatabaseStmt>();
    node->name = consume(TokenType::IDENT).value;
    return node;
}

ASTNodePtr Parser::parseUse() {
    consume(TokenType::K_USE);
    auto node  = std::make_unique<UseStmt>();
    node->name = consume(TokenType::IDENT).value;
    return node;
}

// CREATE TABLE name (col TYPE CONSTRAINT, ...)
ASTNodePtr Parser::parseCreateTable() {
    consume(TokenType::K_TABLE);
    auto node   = std::make_unique<CreateTableStmt>();
    node->table = parseTableRef();
    consume(TokenType::SYM_LPAREN);
    node->columns.push_back(parseColumnDef());
    while (match(TokenType::SYM_COMMA))
        node->columns.push_back(parseColumnDef());
    consume(TokenType::SYM_RPAREN);
    return node;
}

ASTNodePtr Parser::parseDropTable() {
    consume(TokenType::K_TABLE);
    auto node   = std::make_unique<DropTableStmt>();
    node->table = parseTableRef();
    return node;
}

// INSERT INTO table (cols) VALUE (vals), ...
ASTNodePtr Parser::parseInsert() {
    consume(TokenType::K_INSERT);
    consume(TokenType::K_INTO);
    auto node   = std::make_unique<InsertStmt>();
    node->table = parseTableRef();

    consume(TokenType::SYM_LPAREN);
    node->columns.push_back(consume(TokenType::IDENT).value);
    while (match(TokenType::SYM_COMMA))
        node->columns.push_back(consume(TokenType::IDENT).value);
    consume(TokenType::SYM_RPAREN);

    consume(TokenType::K_VALUE);

    // парсим одну строку значений ( val, val, ... )
    auto parseOneRow = [&]() {
        std::vector<Value> row;
        consume(TokenType::SYM_LPAREN);
        row.push_back(parseValue());
        while (match(TokenType::SYM_COMMA))
            row.push_back(parseValue());
        consume(TokenType::SYM_RPAREN);
        return row;
    };

    node->rows.push_back(parseOneRow());
    while (match(TokenType::SYM_COMMA))
        node->rows.push_back(parseOneRow());

    return node;
}

// UPDATE table SET col=val, ... WHERE condition
ASTNodePtr Parser::parseUpdate() {
    consume(TokenType::K_UPDATE);
    auto node   = std::make_unique<UpdateStmt>();
    node->table = parseTableRef();
    consume(TokenType::K_SET);

    auto parseAssignment = [&]() {
        std::string col = consume(TokenType::IDENT).value;
        consume(TokenType::SYM_EQ); // одиночный = для присваивания, не ==
        return std::make_pair(col, parseValue());
    };

    node->assignments.push_back(parseAssignment());
    while (match(TokenType::SYM_COMMA))
        node->assignments.push_back(parseAssignment());

    node->where = match(TokenType::K_WHERE) ? parseExpr() : nullptr;
    return node;
}

// DELETE FROM table WHERE condition
ASTNodePtr Parser::parseDelete() {
    consume(TokenType::K_DELETE);
    consume(TokenType::K_FROM);
    auto node   = std::make_unique<DeleteStmt>();
    node->table = parseTableRef();
    node->where = match(TokenType::K_WHERE) ? parseExpr() : nullptr;
    return node;
}

// SELECT * → columns в ноде пустой (сигнал для executor'а взять все)
// SELECT col1, col2 → columns заполнен; SELECT без * и без колонок — SyntaxError
ASTNodePtr Parser::parseSelect() {
    consume(TokenType::K_SELECT);
    auto node = std::make_unique<SelectStmt>();

    if (!match(TokenType::SYM_STAR)) {
        // SELECT col [AS alias], ...
        auto parseCol = [&]() {
            SelectColumn sc;
            sc.name  = consume(TokenType::IDENT).value;
            sc.alias = std::nullopt;
            if (match(TokenType::K_AS))
                sc.alias = consume(TokenType::IDENT).value;
            return sc;
        };
        node->columns.push_back(parseCol());
        while (match(TokenType::SYM_COMMA))
            node->columns.push_back(parseCol());
    }

    consume(TokenType::K_FROM);
    node->table = parseTableRef();
    node->where = match(TokenType::K_WHERE) ? parseExpr() : nullptr;
    return node;
}

// парсит table или db.table
TableReference Parser::parseTableRef() {
    Token first = consume(TokenType::IDENT);
    if (match(TokenType::SYM_DOT)) {
        Token second = consume(TokenType::IDENT);
        return TableReference{ first.value, second.value };
    }
    return TableReference{ std::nullopt, first.value };
}

// имя колонки + тип (INT/STRING) + опциональный constraint
ColumnDef Parser::parseColumnDef() {
    ColumnDef def;
    def.name = consume(TokenType::IDENT).value;

    if      (match(TokenType::K_INT))    def.type = ColumnType::INT;
    else if (match(TokenType::K_STRING)) def.type = ColumnType::STRING;
    else throw SyntaxError(
        "Строка " + std::to_string(current().line) +
        ": ожидался тип колонки INT или STRING"
    );

    def.constraint = Constraint::NONE;
    if      (match(TokenType::K_NOT_NULL)) def.constraint = Constraint::NOT_NULL;
    else if (match(TokenType::K_INDEXED))  def.constraint = Constraint::INDEXED;

    return def;
}

// число, строка или NULL
Value Parser::parseValue() {
    if (check(TokenType::LIT_INT)) {
        Token t = consume(TokenType::LIT_INT);
        int result;
        auto [ptr, ec] = std::from_chars(t.value.data(), t.value.data() + t.value.size(), result);
        if (ec != std::errc{})
            throw SyntaxError("Строка " + std::to_string(t.line) + ": число '" + t.value + "' выходит за пределы int");
        return result;
    }
    if (check(TokenType::LIT_STRING))
        return consume(TokenType::LIT_STRING).value;
    if (match(TokenType::K_NULL))
        return std::monostate{};
    throw SyntaxError(
        "Строка " + std::to_string(current().line) +
        ": ожидалось значение (число, строка или NULL)"
    );
}

// наименьший приоритет - собирает AND/OR цепочки
ExprPtr Parser::parseExpr() {
    ExprPtr left = parseComparison();

    while (check(TokenType::K_AND) || check(TokenType::K_OR)) {
        LogicalOper op;
        if      (match(TokenType::K_AND)) op = LogicalOper::AND;
        else if (match(TokenType::K_OR))  op = LogicalOper::OR;
        else break; // недостижимо — while гарантирует AND или OR

        ExprPtr right = parseComparison();
        auto node     = std::make_unique<LogicalExpr>();
        node->left    = std::move(left);
        node->oper    = op;
        node->right   = std::move(right);
        left          = std::move(node);
    }
    return left;
}

// операторы сравнения, BETWEEN, LIKE
ExprPtr Parser::parseComparison() {
    ExprPtr left = parsePrimary();

    if (match(TokenType::K_BETWEEN)) {
        ExprPtr lo = parsePrimary();
        consume(TokenType::K_AND); // AND внутри BETWEEN съедается здесь, не доходит до parseExpr
        ExprPtr hi  = parsePrimary();
        auto node   = std::make_unique<BetweenExpr>();
        node->value = std::move(left);
        node->low   = std::move(lo);
        node->high  = std::move(hi);
        return node;
    }

    if (match(TokenType::K_LIKE)) {
        auto node     = std::make_unique<LikeExpr>();
        node->value   = std::move(left);
        node->pattern = consume(TokenType::LIT_STRING).value;
        return node;
    }

    BinaryOper op = BinaryOper::EQ; // перезаписывается ниже; инициализация против предупреждений компилятора
    bool found = true;
    if      (match(TokenType::OP_EQ))  op = BinaryOper::EQ;
    else if (match(TokenType::OP_NEQ)) op = BinaryOper::NEQ;
    else if (match(TokenType::OP_LT))  op = BinaryOper::LT;
    else if (match(TokenType::OP_GT))  op = BinaryOper::GT;
    else if (match(TokenType::OP_LTE)) op = BinaryOper::LTE;
    else if (match(TokenType::OP_GTE)) op = BinaryOper::GTE;
    else found = false;

    if (found) {
        ExprPtr right = parsePrimary();
        auto node     = std::make_unique<BinaryExpr>();
        node->left    = std::move(left);
        node->oper    = op;
        node->right   = std::move(right);
        return node;
    }

    return left;
}

// нижний уровень выражения: конкретное значение, имя колонки или (выражение в скобках)
ExprPtr Parser::parsePrimary() {
    if (match(TokenType::SYM_LPAREN)) {
        ExprPtr inner = parseExpr();
        consume(TokenType::SYM_RPAREN);
        return inner;
    }

    if (check(TokenType::LIT_INT)) {
        Token t = consume(TokenType::LIT_INT);
        int result;
        auto [ptr, ec] = std::from_chars(t.value.data(), t.value.data() + t.value.size(), result);
        if (ec != std::errc{})
            throw SyntaxError("Строка " + std::to_string(t.line) + ": число '" + t.value + "' выходит за пределы int");
        auto node   = std::make_unique<LiteralExpr>();
        node->value = result;
        return node;
    }

    if (check(TokenType::LIT_STRING)) {
        auto node   = std::make_unique<LiteralExpr>();
        node->value = consume(TokenType::LIT_STRING).value;
        return node;
    }

    if (match(TokenType::K_NULL)) {
        auto node   = std::make_unique<LiteralExpr>();
        node->value = std::monostate{};
        return node;
    }

    if (check(TokenType::IDENT)) {
        auto node  = std::make_unique<ColumnRefExpr>();
        node->name = consume(TokenType::IDENT).value;
        return node;
    }

    throw SyntaxError(
        "Строка " + std::to_string(current().line) +
        ": ожидалось выражение (идентификатор, число, строка, NULL или '(')"
    );
}

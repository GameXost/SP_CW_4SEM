//
// Created by GameXost on 11.04.2026.
//

#ifndef CW_EXPR_H
#define CW_EXPR_H
#include <memory>
#include <string>
#include "core/types.h"


// базовая структура для всех узлов выражений
struct ExprNode {
    virtual ~ExprNode() = default;
};

// псевдоним, чтоб каждый раз не писать std::unique_ptr<ExprNode>
using ExprPtr = std::unique_ptr<ExprNode>;

// Значение из запроса: (конкретные числа/строки)
struct LiteralExpr :ExprNode {
    Value value; // из src/core/types.h
};


// ссылка на колонку: имя из запроса (age, name, id или др)
struct ColumnRefExpr : ExprNode {
    std::string name;
};


// бинарные операции
enum class BinaryOper {
    EQ,  // ==
    NEQ, // !=
    LT,  // <
    GT,  // >
    LTE, // <=
    GTE  // >=
};


// бинарное выражение
struct BinaryExpr : ExprNode {
    ExprPtr left;
    BinaryOper oper;
    ExprPtr right;
};


// промежутки [low, high) девая граница включительна, правая не включительно
struct BetweenExpr : ExprNode {
    ExprPtr value;
    ExprPtr low;
    ExprPtr high;
};


// LIKE: value LIKE "regex"
// name LIKE "A.*"
struct LikeExpr : ExprNode {
    ExprPtr value;
    std::string pattern;
};


// логические операторы: left AND/OR right
// age > 18 AND name LIKE "A.*"
enum class LogicalOper {AND, OR};

struct LogicalExpr : ExprNode {
    ExprPtr left;
    LogicalOper oper;
    ExprPtr right;
};


#endif //CW_EXPR_H
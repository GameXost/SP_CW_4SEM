//
// Created by GameXost on 12.04.2026.
//

#ifndef CW_TOKEN_H
#define CW_TOKEN_H

#include <string>
enum class TokenType {
    // KEY WORDS - ключевые слова
    K_CREATE,
    K_DROP,
    K_USE,
    K_DATABASE,
    K_TABLE,
    K_INSERT,
    K_INTO,
    K_VALUE,
    K_UPDATE,
    K_SET,
    K_DELETE,
    K_FROM,
    K_SELECT,
    K_WHERE,
    K_AND,
    K_OR,
    K_NOT_NULL,
    K_INDEXED,
    K_AS,
    K_BETWEEN,
    K_LIKE,
    K_NULL,

    // типы данных
    K_INT,
    K_STRING,

    // operations - операции
    OP_EQ,
    OP_NEQ,
    OP_LT,
    OP_GT,
    OP_LTE,
    OP_GTE,

    //symbols - символы
    SYM_LPAREN,
    SYM_RPAREN,
    SYM_COMMA,
    SYM_SEMICOLON,
    SYM_DOT,
    SYM_STAR,
    SYM_EQ,

    //literals - значения
    LIT_INT,
    LIT_STRING,

    //идентификатор - имена: users, age....
    IDENT,

    // служебка - конец файла
    END_OF_FILE,
};

struct Token {
    TokenType type;
    std::string value; // текст токена как в запросе
    int line; // номер строки - для логов
};

#endif //CW_TOKEN_H
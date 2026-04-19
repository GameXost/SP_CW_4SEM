//
// Created by GameXost on 19.04.2026.
//


#ifndef CW_PARSER_H
#define CW_PARSER_H
#include <vector>
#include "parser/ast.h"
#include "parser/token.h"


class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    ASTNodePtr parse();
private:
    std::vector<Token> _vector;
    size_t _position = 0;


    Token &current(); // возвращает текущий токен, без сдвига
    Token &peek(size_t offset = 1); // возвращает токен с определенным смещением, не двигаем позицию
    Token consume(TokenType expected); // обработка токена, ожидаемый == полученный, то двигаемся дальше, нет - ошибка
    bool match(TokenType type); // менее строгая проверка токена для необязательных частей, со сдвигом

    // общие обработчики, распределяют по уровням выражений
    ASTNodePtr parseStatement();
    ASTNodePtr parseCreate();
    ASTNodePtr parseDrop();

    ASTNodePtr parseCreateDatabase();
    ASTNodePtr parseDropDatabase();

    ASTNodePtr parseCreateTable();
    ASTNodePtr parseDropTable();

    ASTNodePtr parseUse();

    ASTNodePtr parseUpdate();
    ASTNodePtr parseInsert();
    ASTNodePtr parseDelete();
    ASTNodePtr parseSelect();

    bool check(TokenType type); // используется внутри match/consume для проверки типа токена
    TableReference parseTableRef(); // парсит название таблицы, т.к. таблица может быть передана 2 способами
    ColumnDef parseColumnDef(); // парсит колонку
    Value parseValue(); // парсит значение

    // парсят операции по приоритетам
    ExprPtr parseExpr();
    ExprPtr parseComparison();
    ExprPtr parsePrimary();
};


#endif //CW_PARSER_H
//
// Created by GameXost on 12.04.2026.
//

#ifndef CW_LEXER_H
#define CW_LEXER_H

#include <string>
#include <vector>
#include "parser/token.h"

class Lexer {
public:
    //принимает весь текст запроса
    explicit Lexer(const std::string &input);

    // основной метод для возврата всех токенов сразу
    std::vector<Token> tokenize();

private:
    std::string _input; // исходная строка
    size_t _position; // текущая позиция в строке
    int _line; // текущая строка

    char current() const; // возвращает текущий символ
    char nextPos() const; // след символ, не двигает position
    void moveNextPos(); // двиг position на +1;
    void skipSpace(); // пропустить пробелы и знаки переноса\табуляции

    Token readIdent(); // чтение целого слова keyword/ident
    Token readNumber(); // чтение цифры - LIT_INT
    Token readString(); // чтение строки  - LIT_STRING
    Token readOperator(); // чтение операторов(= == != < <= > >=)

};



#endif //CW_LEXER_H
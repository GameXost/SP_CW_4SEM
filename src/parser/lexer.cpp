//
// Created by GameXost on 12.04.2026.
//


#include "parser/lexer.h"
#include <cctype>
#include <stdexcept>
#include <algorithm>
#include <unordered_map>


static const std::unordered_map<std::string, TokenType> KEYWORDS = {
    {"CREATE",   TokenType::K_CREATE},
    {"DROP",     TokenType::K_DROP},
    {"USE",      TokenType::K_USE},
    {"DATABASE", TokenType::K_DATABASE},
    {"TABLE",    TokenType::K_TABLE},
    {"INSERT",   TokenType::K_INSERT},
    {"INTO",     TokenType::K_INTO},
    {"VALUE",    TokenType::K_VALUE},
    {"UPDATE",   TokenType::K_UPDATE},
    {"SET",      TokenType::K_SET},
    {"DELETE",   TokenType::K_DELETE},
    {"FROM",     TokenType::K_FROM},
    {"SELECT",   TokenType::K_SELECT},
    {"WHERE",    TokenType::K_WHERE},
    {"AND",      TokenType::K_AND},
    {"OR",       TokenType::K_OR},
    {"NOT_NULL", TokenType::K_NOT_NULL},
    {"INDEXED",  TokenType::K_INDEXED},
    {"AS",       TokenType::K_AS},
    {"BETWEEN",  TokenType::K_BETWEEN},
    {"LIKE",     TokenType::K_LIKE},
    {"NULL",     TokenType::K_NULL},
    {"INT",      TokenType::K_INT},
    {"STRING",   TokenType::K_STRING},
};

Lexer::Lexer(const std::string &input)
    : _input(input), _position(0), _line(1){}

char Lexer::current() const {
    if (_position >= _input.size()) return '\0';
    return _input[_position];
}

char Lexer::nextPos() const {
    if (_position + 1 >= _input.size()) return '\0';
    return _input[_position+1];
}

void Lexer::moveNextPos() {
    if (_position < _input.size()) {
        if (current() == '\n') _line++;
        _position++;
    }
}

void Lexer::skipSpace() {
    while (_position < _input.size() && std::isspace(current())) moveNextPos();
}

Token Lexer::readIdent() {
    int start_line = _line;
    std::string word;
    while (std::isalnum(current()) || current()=='_') {
        word += current();
        moveNextPos();
    }
    std::string upper = word;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    auto lit = KEYWORDS.find(upper);
    if (lit != KEYWORDS.end()) return {lit->second, word, start_line};
    return {TokenType::IDENT, word, start_line};
}


Token Lexer::readNumber() {
    int start_line = _line;
    std::string num;
    while (std::isdigit(current())) {
        num += current();
        moveNextPos();
    }
    return {TokenType::LIT_INT, num, start_line};
}


Token Lexer::readString() {
    int start_line = _line;
    std::string str;
    moveNextPos(); // скипаем "
    while (current() != '"' && current() != '\0') {
        str += current();
        moveNextPos();
    }
    if (current() == '\0') {
        throw std::runtime_error("кавычку на закрытие проебали" + std::to_string(start_line));
    }
    moveNextPos();
    return {TokenType::LIT_STRING, str, start_line};
}

Token Lexer::readOperator() {
    int start_line = _line;
    char c = current();
    moveNextPos();
    switch (c) {
        case '(': return {TokenType::SYM_LPAREN, "(", start_line};
        case ')': return {TokenType::SYM_RPAREN, ")", start_line};
        case ',': return {TokenType::SYM_COMMA, ",", start_line};
        case ';': return {TokenType::SYM_SEMICOLON, ";", start_line};
        case '.': return {TokenType::SYM_DOT, ".", start_line};
        case '*': return {TokenType::SYM_STAR, "*", start_line};
        case '=':
            if (current() == '=' ) {
                moveNextPos();
                return {TokenType::OP_EQ, "==", start_line};
            }
            return {TokenType::SYM_EQ, "=", start_line};
        case '!':
            if (current() == '=') {
                moveNextPos();
                return {TokenType::OP_NEQ, "!=", start_line};
            }
        case '<':
            if (current() == '=') {
                moveNextPos();
                return {TokenType::OP_LTE,"<=", start_line};
            }
            return {TokenType::OP_LT, "<", start_line};
        case '>':
            if (current() == '=') {
                moveNextPos();
                return {TokenType::OP_GTE, ">=", start_line};
            }
            return {TokenType::OP_GT, ">", start_line};
        default:
            throw std::runtime_error(std::string("unknown symbol '") + c + "' on str: " + std::to_string(start_line));
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        skipSpace();
        if (current() == '\0') {
            tokens.push_back({TokenType::END_OF_FILE, "", _line});
            break;
        }
        if      (std::isalpha(current()) || current() == '_') tokens.push_back(readIdent());
        else if (std::isdigit(current())) tokens.push_back(readNumber());
        else if ( current() == '"') tokens.push_back(readString());
        else    tokens.push_back(readOperator());
    }
    return tokens;
}










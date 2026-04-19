//
// Created by GameXost on 19.04.2026.
//

#include "parser.h"

#include <complex>

Parser::Parser(std::vector<Token> tokens)
    :_vector(std::move(tokens)), _position(0){}

Token &Parser::current() {
    return _vector[_position];
}

Token &Parser::peek(size_t offset) {
    size_t new_pos = _position + offset;
    if (new_pos >= _vector.size()) return _vector.back();
    return _vector[new_pos];
}

Token Parser::consume(TokenType expected) {
    if (!check(expected)) {
        throw std::runtime_error("Строка " + std::to_string(current().line) +
            ": ожидался токен " + std::to_string(static_cast<int>(expected)) +
            ", найден '" + current().value + "'"
        );
    }
    Token token = current();
    _position++;
    return token;
}

bool Parser::match(TokenType type) {
    if (!check(type)) return false;
    _position++;
    return true;
}


bool Parser::check(TokenType type) {
    return current().type == type;
}



ASTNodePtr Parser::parse() {
    ASTNodePtr node = parseStatement();
    consume(TokenType::SYM_SEMICOLON);
    return node;
}



ASTNodePtr Parser::parseStatement() {
    switch (current().type) {
        case TOk
    }
}


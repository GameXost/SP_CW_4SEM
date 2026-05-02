//
// Created by GameXost on 03.04.2026.
//

#ifndef SP_CW_4SEM_EXCEPTIONS_H
#define SP_CW_4SEM_EXCEPTIONS_H

#include <string>
#include <stdexcept>

struct SyntaxError : std::runtime_error {
    explicit SyntaxError(const std::string& msg) : std::runtime_error(msg) {}
};

struct SemanticError : std::runtime_error {
    explicit SemanticError(const std::string& msg) : std::runtime_error(msg) {}
};

struct TypeError : std::runtime_error {
    explicit TypeError(const std::string& msg) : std::runtime_error(msg) {}
};

struct ConstraintError : std::runtime_error {
    explicit ConstraintError(const std::string& msg) : std::runtime_error(msg) {}
};

#endif //SP_CW_4SEM_EXCEPTIONS_H
#pragma once
#include <string>
#include <optional>

// Результат возвращаемый экзекутором
struct ExecuteResult {
    bool        ok      = true;
    std::string message;
    std::optional<std::string> data;
};
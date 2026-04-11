//
// Created by GameXost on 03.04.2026.
//

// Общие типы, структуры и т.п

#ifndef SP_CW_4SEM_TYPES_H
#define SP_CW_4SEM_TYPES_H

#include  <string>
#include <variant>
#include <optional>

// Тип данных колонки
enum class ColumnType {
    INT,
    STRING
};

// ограничение значений в колонках
enum class Constraint {
    NONE,           // обычная колонка, может быть null
    NOT_NULL,       // низя быть null
    INDEXED         // уникальное поле ну и индекс деревянный, null запрещен
};

// значение ячейки: null/int/string
// monostate = null
// по сути union, но с возможностью ничего не хранить - monostate
using Value = std::variant<std::monostate, int, std::string>;

// ссылка на таблицу из запроса, по сути просто её название
// users    => db = пустое, table = users
// db.users => db = db, table = users
struct TableReference {
    std::optional<std::string> db; // optional - может и не быть этого значения, типа: "db"/{}
    std::string table;
};
#endif //SP_CW_4SEM_TYPES_H
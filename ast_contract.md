# AST Contract - Parser Output Specification

Описывает структуры данных, которые parser отдает на исполнение.  
Parser возвращает `std::unique_ptr<ASTNode>` - указатель на корень дерева.  
Executor вызывает `node->accept(visitor)` и обрабатывает конкретный тип.


## 1. Примитивы (`src/core/`)

Общие типы для всего проекта.

```cpp
// Тип данных колонки
enum class ColumnType {
    INT,
    STRING
};

// Ограничения колонки
enum class Constraint {
    NONE,
    NOT_NULL,   // запрет NULL
    INDEXED     // уникальность + B*-индекс
};

// Значение ячейки: NULL | int | string
// std::monostate = NULL
using Value = std::variant<std::monostate, int, std::string>;

// Ссылка на таблицу: "table" или "db.table"
struct TableRef {
    std::optional<std::string> db;   // отсутствует если USE уже выбрал контекст
    std::string                table;
};
```

---

## 2. Узлы выражений - дерево условий WHERE (`src/parser/expr.h`)

Базовый класс:

```cpp
struct ExprNode {
    virtual ~ExprNode() = default;
};
```

### 2.1 Литерал

```cpp
struct LiteralExpr : ExprNode {
    Value value;   // 42 | "hello" | NULL (monostate)
};
```

Примеры: `42`, `"Alice"`, `NULL`

### 2.2 Ссылка на колонку

```cpp
struct ColumnRefExpr : ExprNode {
    std::string name;   // "age", "name"
};
```

### 2.3 Бинарное сравнение

```cpp
enum class BinaryOp { EQ, NEQ, LT, GT, LTE, GTE };
//                    ==  !=   <   >   <=   >=

struct BinaryExpr : ExprNode {
    std::unique_ptr<ExprNode> left;
    BinaryOp                  op;
    std::unique_ptr<ExprNode> right;
};
```

Пример: `age > 18`
```
BinaryExpr
├── left:  ColumnRefExpr { "age" }
├── op:    GT
└── right: LiteralExpr { 18 }
```

### 2.4 BETWEEN

```cpp
struct BetweenExpr : ExprNode {
    std::unique_ptr<ExprNode> value;
    std::unique_ptr<ExprNode> lo;    // нижняя граница (включительно)
    std::unique_ptr<ExprNode> hi;    // верхняя граница (не включается)
};
```

Пример: `age BETWEEN 18 AND 65` → интервал `[18, 65)`

### 2.5 LIKE (регулярное выражение)

```cpp
struct LikeExpr : ExprNode {
    std::unique_ptr<ExprNode> value;
    std::string               pattern;   // regex-строка
};
```

Пример: `name LIKE "A.*"`

### 2.6 Логические операторы AND / OR

```cpp
enum class LogicOp { AND, OR };

struct LogicalExpr : ExprNode {
    std::unique_ptr<ExprNode> left;
    LogicOp                   op;
    std::unique_ptr<ExprNode> right;
};
```

Пример: `age > 18 AND name LIKE "A.*"`
```
LogicalExpr (AND)
├── BinaryExpr (GT)   → age > 18
└── LikeExpr          → name LIKE "A.*"
```

---

## 3. Узлы команд - Statement nodes (`src/parser/ast.h`)

### Базовый класс + Visitor

```cpp
struct Visitor; // forward declaration

struct ASTNode {
    virtual void accept(Visitor&) = 0;
    virtual ~ASTNode() = default;
};
```

Visitor - интерфейс, который реализует экзекьютор:

```cpp
struct Visitor {
    virtual void visit(CreateDatabaseStmt&) = 0;
    virtual void visit(DropDatabaseStmt&)   = 0;
    virtual void visit(UseStmt&)            = 0;
    virtual void visit(CreateTableStmt&)    = 0;
    virtual void visit(DropTableStmt&)      = 0;
    virtual void visit(InsertStmt&)         = 0;
    virtual void visit(UpdateStmt&)         = 0;
    virtual void visit(DeleteStmt&)         = 0;
    virtual void visit(SelectStmt&)         = 0;
    virtual ~Visitor() = default;
};
```

---

### 3.1 Работа с базами данных

```cpp
// CREATE DATABASE name;
struct CreateDatabaseStmt : ASTNode {
    std::string name;
    void accept(Visitor& v) override { v.visit(*this); }
};

// DROP DATABASE name;
struct DropDatabaseStmt : ASTNode {
    std::string name;
    void accept(Visitor& v) override { v.visit(*this); }
};

// USE name;
struct UseStmt : ASTNode {
    std::string name;
    void accept(Visitor& v) override { v.visit(*this); }
};
```

---

### 3.2 DDL - схемы таблиц

```cpp
// Описание одной колонки при CREATE TABLE
struct ColumnDef {
    std::string name;
    ColumnType  type;
    Constraint  constraint;   // NONE | NOT_NULL | INDEXED
};

// CREATE TABLE name (col1 TYPE MODIFIER, ...);
struct CreateTableStmt : ASTNode {
    TableRef              table;
    std::vector<ColumnDef> columns;
    void accept(Visitor& v) override { v.visit(*this); }
};

// DROP TABLE name;
struct DropTableStmt : ASTNode {
    TableRef table;
    void accept(Visitor& v) override { v.visit(*this); }
};
```

---

### 3.3 DML - манипулирование данными

```cpp
// INSERT INTO table (col1, col2) VALUE (v1, v2), (v3, v4);
struct InsertStmt : ASTNode {
    TableRef                          table;
    std::vector<std::string>          columns;   // список колонок
    std::vector<std::vector<Value>>   rows;      // одна или несколько строк
    void accept(Visitor& v) override { v.visit(*this); }
};

// UPDATE table SET col1 = val1, col2 = val2 WHERE condition;
struct UpdateStmt : ASTNode {
    TableRef                                    table;
    std::vector<std::pair<std::string, Value>>  assignments;   // SET-часть
    std::unique_ptr<ExprNode>                   where;         // nullptr если нет WHERE
    void accept(Visitor& v) override { v.visit(*this); }
};

// DELETE FROM table WHERE condition;
struct DeleteStmt : ASTNode {
    TableRef                  table;
    std::unique_ptr<ExprNode> where;   // nullptr если нет WHERE
    void accept(Visitor& v) override { v.visit(*this); }
};
```

---

### 3.4 SELECT

```cpp
// Одна колонка в SELECT: "name", "name AS alias", или * (пустой вектор)
struct SelectColumn {
    std::string                name;    // имя колонки
    std::optional<std::string> alias;   // AS alias, если есть
};

// SELECT */cols FROM table WHERE condition;
// Если columns пустой - SELECT *
struct SelectStmt : ASTNode {
    TableRef                   table;
    std::vector<SelectColumn>  columns;   // пустой = SELECT *
    std::unique_ptr<ExprNode>  where;     // nullptr если нет WHERE
    void accept(Visitor& v) override { v.visit(*this); }
};
```

---

## 4. Как экзекьютор работает с деревом

```cpp
// Экзекьютор реализует Visitor
class ExecutorVisitor : public Visitor {
public:
    void visit(SelectStmt& s) override {
        // s.table   → куда идти
        // s.columns → что выбрать
        // s.where   → условие (рекурсивно вычислить)
    }
    void visit(InsertStmt& s) override { /* ... */ }
    // ... остальные
};

// Использование (в main или dispatcher):
std::unique_ptr<ASTNode> ast = parser.parse(query);
ExecutorVisitor executor;
ast->accept(executor);   // диспатч по типу, одна строка
```

---

## 5. Связь с B*-деревом

B*-дерево используется только для колонок с модификатором `INDEXED`.  
Парсер это не знает - он просто проставляет `Constraint::INDEXED` в `ColumnDef`.  
Экзекьютор при обработке `CreateTableStmt` видит `INDEXED` и создаёт индекс.  
При `SelectStmt` экзекьютор проверяет, есть ли индекс для колонки из `where`, и использует его.

```
ColumnDef { name: "id", type: INT, constraint: INDEXED }
                                         ↓
                              Executor создаёт B*-дерево
                              ключ → RecordId (адрес в файле)
```

---

## 6. Что парсер не делает

- Не проверяет существование таблиц/баз - это семантика, задача экзекьютора
- Не проверяет типы значений - экзекьютор сверяет с `ColumnDef`
- Не работает с файлами и индексами

Парсер отвечает только за **грамматическую корректность** запроса.  
Если запрос синтаксически неверен - бросает исключение с описанием ошибки.
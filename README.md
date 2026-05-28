# mini-db

Учебная СУБД на C++20 с собственным SQL-подобным языком: запрос разбирается,
исполняется и складывается на диск. \
Курсовая по системному программированию
(4 семестр).\
Индекс — B*-tree.

## Возможности

- Команды: `CREATE/DROP DATABASE`, `USE`, `CREATE/DROP TABLE`, `INSERT`, `UPDATE`, `DELETE`, `SELECT`
- Типы: `INT`, `STRING` (NULL = отсутствие значения)
- Ограничения: `NOT_NULL`, `INDEXED` (уникальность + автоиндекс B*-tree)
- `DEFAULT`-значения в `CREATE TABLE`
- Агрегаты `SUM` / `COUNT` / `AVG`
- `WHERE` с `==/!=/</>/<=/>=`, `AND`/`OR`, `BETWEEN`, `LIKE`, скобками
- Хранение на диске слотовыми страницами по 4 КБ, результат `SELECT` — JSON
- Два режима: интерактивный и пакетный (скрипт `.sql`)

## Пайплайн

```
SQL-строка
   │
   ▼
 Lexer     строка → токены
   │
   ▼
 Parser    токены → AST
   │
   ▼
 Executor  обход AST, проверка семантики, диспатч
   ├── Catalog   схема (catalog.json)
   ├── Storage   строки на диске
   └── Index     B*-tree по INDEXED-колонкам
   │
   ▼
 результат: JSON (SELECT) или статус
```

## Модули

| Модуль | Путь | Назначение |
|---|---|---|
| Lexer | `src/parser/lexer.*` | строка → `vector<Token>` |
| Parser | `src/parser/parser.*` | токены → `ASTNode` |
| Executor | `src/execution/` | исполняет AST, семантика, упаковка в байты |
| Catalog | `src/catalog/` | схемы БД и таблиц в `catalog.json` |
| Storage | `src/storage/` | запись/чтение строк, слотовые страницы, диск |
| Index | `src/index/` | B*-tree, связывает ключ с адресом строки |
| Core | `src/core/` | общие типы, ошибки, метрики |

## Сборка

```bash
mkdir build && cd build
cmake ..
cmake --build .
./prog
```

Зависимость `nlohmann/json` подтягивается автоматически через `FetchContent`.

## Запуск

```bash
./prog              # интерактивный режим, запросы заканчиваются на ;
./prog script.sql   # пакетный режим: выполнить файл
```

## Тесты

Юнит-тесты на GoogleTest (Lexer / Parser / Executor), по умолчанию выключены:

```bash
cmake -DBUILD_TESTS=ON ..
cmake --build . --target cw_tests
ctest            # или ./cw_tests
```

Готовые SQL-скрипты для ручной проверки лежат в `tests/*.sql`.

## Пример

```sql
CREATE DATABASE shop;
USE shop;
CREATE TABLE users (id INT INDEXED, name STRING NOT_NULL, age INT DEFAULT 0);
INSERT INTO users (id, name, age) VALUE (1, "Alice", 30), (2, "Bob", 25);
SELECT name FROM users WHERE age > 18;
SELECT AVG(age) FROM users;
```

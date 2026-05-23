//
// Created by GameXost on 23.03.2026.
//

#include <iostream>
#include <fstream>
#include <string>
#include <cctype>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

#include "parser/lexer.h"
#include "parser/parser.h"
#include "catalog/catalog.h"
#include "storage/storage.h"
#include "storage/pager/pager.h"
#include "execution/executor.h"
#include "core/result.h"

// поиск ";" вне строкового литерала. внутри "..." точка с запятой - часть данных
static size_t findSemicolon(const std::string& buf) {
    bool in_string = false;
    for (size_t i = 0; i < buf.size(); ++i) {
        char c = buf[i];
        if (in_string) {
            if (c == '"') in_string = false;
        } else if (c == '"') {
            in_string = true;
        } else if (c == ';') {
            return i;
        }
    }
    return std::string::npos;
}

static bool isBlank(const std::string& s) {
    for (char c : s) if (!std::isspace((unsigned char)c)) return false;
    return true;
}

// лексер -> парсер -> executor -> принт
static void runQuery(const std::string& sql, Executor& exec) {
    try {
        Lexer lexer(sql);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        ASTNodePtr node = parser.parse();

        ExecuteResult result = exec.execute(*node);

        if (!result.ok) {
            std::cout << "Error: " << result.message << '\n';
        } else if (!result.data.is_null()) {
            // SELECT - data это JSON-массив строк
            std::cout << result.data.dump(2) << '\n';
        } else if (!result.message.empty()) {
            // DDL и DML - просто сообщение об успехе
            std::cout << result.message << '\n';
        }
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << '\n';
    }
}

// читает поток построчно, копит в буфер до ;, отдаёт запрос за запросом
static void runStream(std::istream& in, Executor& exec, bool interactive) {
    std::string buffer;
    std::string line;
    while (true) {
        if (interactive) std::cout << "SQL> " << std::flush;
        if (!std::getline(in, line)) break;

        // выход в интерактивном режиме
        if (interactive && (line == "exit" || line == "quit")) break;

        if (!buffer.empty()) buffer += '\n';
        buffer += line;

        // сразу несколько запросов
        size_t pos = findSemicolon(buffer);
        while (pos != std::string::npos) {
            std::string query = buffer.substr(0, pos + 1);
            buffer.erase(0, pos + 1);

            if (!isBlank(query.substr(0, query.size() - 1))) {
                runQuery(query, exec);
            }
            pos = findSemicolon(buffer);
        }
    }
}

int main(int argc, char** argv) {
    if (argc > 2) {
        std::cerr << "Usage: prog [script.sql]\n";
        return 1;
    }

    // пока такая заглушка
    std::filesystem::create_directories("./data");

    try {
        Catalog  catalog("catalog.json");
        Storage  storage;
        Executor executor(catalog, storage);

        if (argc == 2) {
            std::ifstream file(argv[1]);
            if (!file) {
                std::cerr << "Error: cannot open '" << argv[1] << "'\n";
                return 1;
            }
            runStream(file, executor, false);
        } else {
            runStream(std::cin, executor, true);
        }
    } catch (const std::exception& e) {
        // catalog не смог распарсить catalog.json или storage не открыл файл
        std::cerr << "Fatal: " << e.what() << '\n';
        return 1;
    }

    return 0;
}

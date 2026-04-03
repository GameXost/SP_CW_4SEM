# При создании нового .cpp файла нужно добавить его путь в блок set(PROJECT_SOURCES)
# Для сборки:
# mkdir build
# cd build
# cmake ..
# make
# ./prog

cmake_minimum_required(VERSION 3.14)


project(CW LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_EXTENSIONS  OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif ()

    # Строгие предупреждения о плохом коде


# для работы с JSON
include(FetchContent)
FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

set(PROJECT_SOURCES
    src/main.cpp
    # и т.д .cpp
)

add_executable(prog ${PROJECT_SOURCES})
# src - корень заголовочных файлов. можно везде писать include "core/types.h"
target_include_directories(prog PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# связка JSON к программе
target_link_libraries(prog PRIVATE nlohmann_json::nlohmann_json)

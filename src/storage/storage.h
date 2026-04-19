#pragma once
#include <vector>
#include <cstdint>
#include <string>


// Заглушка крч
// Физический адрес записи в файле
struct RecordId {
    uint64_t offset = 0;   // смещение в байтах от начала файла
    uint32_t size   = 0;   // размер записи в байтах
};

// Интерфейс хранилища — реализуется в storage.cpp
// Executor использует только эти методы
class Storage {
public:
    virtual ~Storage() = default;

    // Записать байты, вернуть физический адрес
    virtual RecordId write(const std::string& db, const std::string& table,
                           const std::vector<uint8_t>& data) = 0;

    // Прочитать байты по адресу
    virtual std::vector<uint8_t> read(const std::string& db, const std::string& table,
                                      RecordId id) = 0;

    // Полный scan: все записи таблицы (адрес + байты)
    virtual std::vector<std::pair<RecordId, std::vector<uint8_t>>>
        scan(const std::string& db, const std::string& table) = 0;

    // Пометить запись удалённой
    virtual void remove(const std::string& db, const std::string& table, RecordId id) = 0;

    // Перезаписать запись (или tombstone + новая запись — на усмотрение impl)
    virtual RecordId update(const std::string& db, const std::string& table,
                            RecordId id, const std::vector<uint8_t>& data) = 0;

    // Создать/удалить файлы таблицы
    virtual void createTable(const std::string& db, const std::string& table) = 0;
    virtual void dropTable  (const std::string& db, const std::string& table) = 0;
    virtual void createDatabase(const std::string& db) = 0;
    virtual void dropDatabase  (const std::string& db) = 0;
};
#include <pqxx/pqxx>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Структура для представления записи игрока
struct PlayerRecord {
    std::string name;
    int score;
    double playTime;
};

// Класс для работы с базой данных
class DatabaseManager {
public:
    explicit DatabaseManager(const std::string& db_url)
        : connection_(db_url) {}

    // Проверка существования таблицы, если таблицы нет — создание
    void EnsureTableExists() {
        pqxx::work txn(connection_);
        pqxx::result result = txn.exec(
            "SELECT EXISTS ("
            "   SELECT FROM information_schema.tables "
            "   WHERE table_schema = 'public' "
            "   AND table_name = 'retired_players'"
            ");"
        );

        if (!result[0][0].as<bool>()) {
            txn.exec(
                "CREATE TABLE retired_players ("
                "   id UUID PRIMARY KEY,"
                "   name VARCHAR(100),"
                "   score INT,"
                "   play_time_ms DOUBLE PRECISION"
                ");"
            );

            txn.exec(
                "CREATE INDEX retired_players_sort_idx ON retired_players("
                "   score DESC,"
                "   play_time_ms ASC,"
                "   name ASC"
                ");"
            );

            std::cout << "Таблица 'retired_players' была создана." << std::endl;
        } else {
            std::cout << "Таблица 'retired_players' уже существует." << std::endl;
        }
        txn.commit();
    }

    // Добавление записи об игроке
    void AddPlayerRecord(const std::string& name, int score, double play_time_ms) {
        try {
            pqxx::work txn(connection_);

            // Генерация UUID
            boost::uuids::uuid id = boost::uuids::random_generator()();
            std::string id_str = boost::uuids::to_string(id);

            txn.exec_params(
                "INSERT INTO retired_players (id, name, score, play_time_ms) "
                "VALUES ($1::UUID, $2, $3, $4)",
                id_str, name, score, play_time_ms
            );

            txn.commit();
        } catch (const std::exception& e) {
            std::cerr << "Ошибка при добавлении записи: " << e.what() << std::endl;
        }
    }

    // Получение списка игроков с постраничным выводом
    std::vector<PlayerRecord> GetPlayerRecords(int start, int maxItems) {
        std::vector<PlayerRecord> records;

        try {
            pqxx::work txn(connection_);

            pqxx::result result = txn.exec_params(
                "SELECT name, score, play_time_ms "
                "FROM retired_players "
                "ORDER BY score DESC, play_time_ms ASC, name ASC "
                "LIMIT $1 OFFSET $2",
                maxItems, start
            );

            for (const auto& row : result) {
                records.emplace_back(PlayerRecord{
                    row["name"].as<std::string>(),
                    row["score"].as<int>(),
                    row["play_time_ms"].as<double>()
                });
            }
        } catch (const std::exception& e) {
            std::cerr << "Ошибка при получении записей: " << e.what() << std::endl;
        }

        return records;
    }

private:
    pqxx::connection connection_;
};

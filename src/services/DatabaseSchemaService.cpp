#include "DatabaseSchemaService.hpp"

#include <drogon/drogon.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace weather_data::services {

namespace {

void waitForDatabaseReady(const drogon::orm::DbClientPtr &db) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);

    std::chrono::milliseconds sleepFor{200};
    while (true) {
        try {
            db->execSqlSync("SELECT 1");
            return;
        } catch (const std::exception &error) {
            if (std::chrono::steady_clock::now() >= deadline) {
                throw;
            }
            LOG_WARN << "Database not ready yet, retrying: " << error.what();
            std::this_thread::sleep_for(sleepFor);
            sleepFor = std::min<std::chrono::milliseconds>(sleepFor * 2, std::chrono::seconds(1));
        }
    }
}

std::string readFile(const std::string &path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open schema file: " + path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void execSqlStatements(const drogon::orm::DbClientPtr &db, const std::string &sql) {
    // Drogon's PostgreSQL client rejects multiple commands in a single prepared
    // statement. Execute simple sql query statements one-by-one.
    std::stringstream ss(sql);
    std::string statement;
    while (std::getline(ss, statement, ';')) {
        statement.erase(0, statement.find_first_not_of(" \n\r\t"));
        if (!statement.empty()) {
            statement.erase(statement.find_last_not_of(" \n\r\t") + 1);
        }
        if (!statement.empty()) {
            db->execSqlSync(statement);
        }
    }
}

} // namespace

void DatabaseSchemaService::ensureSchema(const drogon::orm::DbClientPtr &db,
                                        const std::string &schemaPath) {
    if (!db) {
        throw std::runtime_error("Database client is not initialized");
    }

    if (!std::filesystem::exists(schemaPath)) {
        throw std::runtime_error("Schema file not found: " + schemaPath);
    }

    LOG_INFO << "Ensuring database schema from: " << schemaPath;
    waitForDatabaseReady(db);

    const auto sql = readFile(schemaPath);
    if (sql.empty()) {
        LOG_WARN << "Schema file is empty: " << schemaPath;
        return;
    }

    execSqlStatements(db, sql);
    LOG_INFO << "Database schema ensured.";
}

} // namespace weather_data::services

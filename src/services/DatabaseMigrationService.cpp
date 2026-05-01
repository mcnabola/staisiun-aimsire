#include "DatabaseMigrationService.hpp"
#include <drogon/drogon.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace weather_data::services {

void DatabaseMigrationService::runMigrations(const drogon::orm::DbClientPtr &db, const std::string &migrationsPath) {
    LOG_INFO << "Starting database migrations from: " << migrationsPath;
    
    ensureMigrationsTable(db);
    
    auto pending = getPendingMigrations(db, migrationsPath);
    
    if (pending.empty()) {
        LOG_INFO << "No pending migrations.";
        return;
    }
    
    for (const auto &file : pending) {
        LOG_INFO << "Applying migration: " << file;
        applyMigration(db, file);
    }
    
    LOG_INFO << "Migrations completed successfully.";
}

void DatabaseMigrationService::ensureMigrationsTable(const drogon::orm::DbClientPtr &db) {
    db->execSqlSync(R"(
        CREATE TABLE IF NOT EXISTS schema_migrations (
            id SERIAL PRIMARY KEY,
            version TEXT UNIQUE NOT NULL,
            applied_at TIMESTAMPTZ DEFAULT NOW()
        )
    )");
}

std::vector<std::string> DatabaseMigrationService::getPendingMigrations(const drogon::orm::DbClientPtr &db, const std::string &migrationsPath) {
    std::vector<std::string> allFiles;
    namespace fs = std::filesystem;
    
    if (!fs::exists(migrationsPath)) {
        LOG_WARN << "Migrations directory does not exist: " << migrationsPath;
        return {};
    }

    for (const auto &entry : fs::directory_iterator(migrationsPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".sql") {
            allFiles.push_back(entry.path().filename().string());
        }
    }
    
    std::sort(allFiles.begin(), allFiles.end());
    
    auto result = db->execSqlSync("SELECT version FROM schema_migrations");
    std::vector<std::string> appliedVersions;
    for (const auto &row : result) {
        appliedVersions.push_back(row["version"].as<std::string>());
    }
    
    std::vector<std::string> pending;
    for (const auto &file : allFiles) {
        if (std::find(appliedVersions.begin(), appliedVersions.end(), file) == appliedVersions.end()) {
            pending.push_back((fs::path(migrationsPath) / file).string());
        }
    }
    
    return pending;
}

void DatabaseMigrationService::applyMigration(const drogon::orm::DbClientPtr &db, const std::string &filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open migration file: " + filePath);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string sql = buffer.str();
    
    if (sql.empty()) {
        LOG_WARN << "Empty migration file: " << filePath;
        return;
    }

    // Use a transaction for each migration file
    auto trans = db->newTransaction();
    try {
        // Basic split by semicolon. 
        // Note: This won't work if semicolons are inside strings or comments.
        // For simple schema migrations, this is often sufficient.
        std::stringstream ss(sql);
        std::string statement;
        while (std::getline(ss, statement, ';')) {
            // Trim whitespace
            statement.erase(0, statement.find_first_not_of(" \n\r\t"));
            statement.erase(statement.find_last_not_of(" \n\r\t") + 1);
            
            if (!statement.empty()) {
                trans->execSqlSync(statement);
            }
        }
        
        std::string filename = std::filesystem::path(filePath).filename().string();
        trans->execSqlSync("INSERT INTO schema_migrations (version) VALUES ($1)", filename);
    } catch (const std::exception &e) {
        LOG_ERROR << "Failed to apply migration " << filePath << ": " << e.what();
        throw;
    }
}

} // namespace weather_data::services

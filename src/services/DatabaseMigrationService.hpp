#pragma once

#include <drogon/orm/DbClient.h>
#include <string>
#include <vector>

namespace weather_data::services {

class DatabaseMigrationService {
  public:
    /**
     * @brief Run all pending migrations from the specified directory.
     * 
     * @param db The database client to use.
     * @param migrationsPath Path to the directory containing .sql migration files.
     */
    static void runMigrations(const drogon::orm::DbClientPtr &db, const std::string &migrationsPath);

  private:
    static void ensureMigrationsTable(const drogon::orm::DbClientPtr &db);
    static std::vector<std::string> getPendingMigrations(const drogon::orm::DbClientPtr &db, const std::string &migrationsPath);
    static void applyMigration(const drogon::orm::DbClientPtr &db, const std::string &filePath);
};

} // namespace weather_data::services

#include <drogon/drogon.h>

#include <filesystem>
#include <stdexcept>
#include <string_view>

#include "services/DatabaseMigrationService.hpp"

namespace {

std::string findConfigPath() {
    constexpr std::string_view kCandidates[] = {
        "config/config.json",
        "../config/config.json",
    };

    for (const auto candidate : kCandidates) {
        if (std::filesystem::exists(candidate)) {
            return std::string(candidate);
        }
    }

    throw std::runtime_error(
        "Config file not found. Tried: config/config.json and ../config/config.json");
}

std::string findMigrationsPath() {
    constexpr std::string_view kCandidates[] = {
        "db/migrations",
        "../db/migrations",
    };

    for (const auto candidate : kCandidates) {
        if (std::filesystem::exists(candidate)) {
            return std::string(candidate);
        }
    }
    return "db/migrations"; // Default fallback
}

}  // namespace

int main() {
    drogon::app().loadConfigFile(findConfigPath());
    
    // Run migrations before the event loop starts
    auto db = drogon::app().getDbClient();
    weather_data::services::DatabaseMigrationService::runMigrations(db, findMigrationsPath());

    drogon::app().run();
    return 0;
}

#include <drogon/drogon.h>

#include <filesystem>
#include <stdexcept>
#include <string_view>

#include "services/DatabaseSchemaService.hpp"

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

std::string findSchemaPath() {
    constexpr std::string_view kCandidates[] = {
        "db/migrations/001_initial_schema.sql",
        "../db/migrations/001_initial_schema.sql",
    };

    for (const auto candidate : kCandidates) {
        if (std::filesystem::exists(candidate)) {
            return std::string(candidate);
        }
    }

    throw std::runtime_error(
        "Schema file not found. Tried: db/migrations/001_initial_schema.sql and ../db/migrations/001_initial_schema.sql");
}

}  // namespace

int main() {
    drogon::app().loadConfigFile(findConfigPath());

    // Bootstrap the database schema as soon as the event loop starts. At this
    // point Drogon has initialized its database clients from config.
    drogon::app().registerBeginningAdvice([] {
        try {
            auto db = drogon::app().getDbClient();
            weather_data::services::DatabaseSchemaService::ensureSchema(db, findSchemaPath());
        } catch (const std::exception &error) {
            LOG_ERROR << "Database schema bootstrap failed: " << error.what();
            drogon::app().quit();
        }
    });

    drogon::app().run();
    return 0;
}

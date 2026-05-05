#pragma once

#include <drogon/orm/DbClient.h>
#include <string>

namespace weather_data::services {

class DatabaseSchemaService {
  public:
    static void ensureSchema(const drogon::orm::DbClientPtr &db, const std::string &schemaPath);
};

} // namespace weather_data::services


#include "ReadingRepository.hpp"

#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>

namespace {

drogon::orm::Result execInsertReading(const drogon::orm::DbClientPtr &client,
                                      const CreateReadingRequest &request) {
    auto binder = *client
                  << "INSERT INTO readings (sensor_id, recorded_at, temperature, humidity, wind_speed) "
                     "SELECT id, $2::timestamptz, $3, $4, $5 "
                     "FROM sensors WHERE external_id = $1 "
                     "RETURNING id::text AS id";

    //sql injection safe.
    //prepared statements.

    binder << request.sensorId << request.timestamp;

    if (request.temperature.has_value()) {
        binder << request.temperature.value();
    } else {
        binder << nullptr;
    }

    if (request.humidity.has_value()) {
        binder << request.humidity.value();
    } else {
        binder << nullptr;
    }

    if (request.windSpeed.has_value()) {
        binder << request.windSpeed.value();
    } else {
        binder << nullptr;
    }

    drogon::orm::Result result(nullptr);
    binder << drogon::orm::Mode::Blocking;
    binder >> [&result](const drogon::orm::Result &queryResult) {
        result = queryResult;
    };
    binder.exec();

    return result;
}

}  // namespace

std::string ReadingRepository::createReading(const CreateReadingRequest &request) const {
    auto dbClient = drogon::app().getDbClient();
    auto transaction = dbClient->newTransaction();

    transaction->execSqlSync(
        "INSERT INTO sensors (external_id) VALUES ($1) ON CONFLICT (external_id) DO NOTHING",
        request.sensorId);

    auto result = execInsertReading(transaction, request);
    if (result.empty()) {
        throw std::runtime_error("Failed to insert reading");
    }

    return result[0]["id"].as<std::string>();
}

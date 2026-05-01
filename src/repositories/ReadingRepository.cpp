#include "ReadingRepository.hpp"

#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>

#include <optional>
#include <algorithm>
#include <sstream>
#include <unordered_map>

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

std::string metricToColumn(const std::string &metric) {
    if (metric == "temperature") {
        return "temperature";
    }
    if (metric == "humidity") {
        return "humidity";
    }
    return "wind_speed";
}

std::string statisticToSql(const std::string &statistic) {
    if (statistic == "min") {
        return "MIN";
    }
    if (statistic == "max") {
        return "MAX";
    }
    if (statistic == "sum") {
        return "SUM";
    }
    return "AVG";
}

std::optional<std::pair<std::string, std::string>> resolveLatestRange(
    const drogon::orm::DbClientPtr &client, const std::vector<std::string> &sensorIds) {

    std::string sql =
        "SELECT "
        "TO_CHAR(MAX(r.recorded_at) AT TIME ZONE 'UTC' - INTERVAL '24 hours', "
        "'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS start_range, "
        "TO_CHAR(MAX(r.recorded_at) AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS "
        "end_range "
        "FROM readings r "
        "JOIN sensors s ON s.id = r.sensor_id";

    if (!sensorIds.empty()) {
        sql += " WHERE s.external_id IN (";
        for (size_t index = 0; index < sensorIds.size(); ++index) {
            if (index > 0) {
                sql += ", ";
            }
            sql += "$" + std::to_string(index + 1);
        }
        sql += ")";
    }

    auto binder = *client << sql;
    for (const auto &sensorId : sensorIds) {
        binder << sensorId;
    }

    drogon::orm::Result result(nullptr);
    binder << drogon::orm::Mode::Blocking;
    binder >> [&result](const drogon::orm::Result &queryResult) {
        result = queryResult;
    };
    binder.exec();

    if (result.empty() || result[0]["end_range"].isNull()) {
        return std::nullopt;
    }

    return std::make_pair(result[0]["start_range"].as<std::string>(),
                          result[0]["end_range"].as<std::string>());
}

std::unordered_map<std::string, double> execAggregateQuery(
    const drogon::orm::DbClientPtr &client,
    const std::string &metric,
    const std::string &statistic,
    const std::vector<std::string> &sensorIds,
    const std::string &from,
    const std::string &to) {
    const auto column = metricToColumn(metric);
    const auto aggregate = statisticToSql(statistic);

    std::string sql = "SELECT s.external_id AS sensor_id, " + aggregate + "(r." + column +
                      ") AS value "
                      "FROM readings r "
                      "JOIN sensors s ON s.id = r.sensor_id "
                      "WHERE r." +
                      column +
                      " IS NOT NULL "
                      "AND r.recorded_at >= $1::timestamptz "
                      "AND r.recorded_at <= $2::timestamptz";

    if (!sensorIds.empty()) {
        sql += " AND s.external_id IN (";
        for (size_t index = 0; index < sensorIds.size(); ++index) {
            if (index > 0) {
                sql += ", ";
            }
            sql += "$" + std::to_string(index + 3);
        }
        sql += ")";
    }

    sql += " GROUP BY s.external_id ORDER BY s.external_id";

    // filling in the SQL prepared statements with $1,$2...N
    auto binder = *client << sql;
    binder << from << to;
    for (const auto &sensorId : sensorIds) {
        binder << sensorId;
    }

    drogon::orm::Result result(nullptr);
    binder << drogon::orm::Mode::Blocking;
    binder >> [&result](const drogon::orm::Result &queryResult) {
        result = queryResult;
    };
    binder.exec();

    std::unordered_map<std::string, double> valuesBySensor;
    for (const auto &row : result) {
        valuesBySensor.emplace(row["sensor_id"].as<std::string>(), row["value"].as<double>());
    }

    return valuesBySensor;
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

MetricsQueryResult ReadingRepository::queryMetrics(const MetricsQueryRequest &request) const {
    auto dbClient = drogon::app().getDbClient();

    std::optional<std::string> resolvedFrom = request.from;
    std::optional<std::string> resolvedTo = request.to;

    if (!resolvedFrom.has_value() || !resolvedTo.has_value()) {
        const auto latestRange = resolveLatestRange(dbClient, request.sensorIds);
        if (!latestRange.has_value()) {
            return MetricsQueryResult{
                .statistic = request.statistic,
                .from = std::nullopt,
                .to = std::nullopt,
                .results = {},
            };
        }

        resolvedFrom = latestRange->first;
        resolvedTo = latestRange->second;
    }

    std::unordered_map<std::string, SensorMetricsResult> resultsBySensor;
    for (const auto &metric : request.metrics) {
        const auto valuesBySensor = execAggregateQuery(
            dbClient,
            metric,
            request.statistic,
            request.sensorIds,
            resolvedFrom.value(),
            resolvedTo.value());

        for (const auto &[sensorId, value] : valuesBySensor) {
            auto &entry = resultsBySensor[sensorId];
            entry.sensorId = sensorId;
            entry.metrics[metric] = value;
        }
    }

    MetricsQueryResult response{
        .statistic = request.statistic,
        .from = resolvedFrom,
        .to = resolvedTo,
        .results = {},
    };

    response.results.reserve(resultsBySensor.size());
    for (auto &[sensorId, entry] : resultsBySensor) {
        response.results.push_back(std::move(entry));
    }

    std::sort(
        response.results.begin(),
        response.results.end(),
        [](const SensorMetricsResult &left, const SensorMetricsResult &right) {
            return left.sensorId < right.sensorId;
        });

    return response;
}

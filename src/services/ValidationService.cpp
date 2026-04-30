#include "ValidationService.hpp"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string_view>

const std::unordered_set<std::string> ValidationService::kSupportedMetrics = {
    "temperature",
    "humidity",
    "windSpeed",
};

const std::unordered_set<std::string> ValidationService::kSupportedStatistics = {
    "min",
    "max",
    "sum",
    "avg",
};

const std::unordered_set<std::string> ValidationService::kAllowedReadingFields = {
    "sensorId",
    "timestamp",
    "temperature",
    "humidity",
    "windSpeed",
};

namespace {

bool isValidUtcTimestamp(const std::string &timestamp) {
    static const std::regex kTimestampPattern(
        R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$)");
    return std::regex_match(timestamp, kTimestampPattern);
}

std::optional<std::time_t> parseUtcTimestamp(const std::string &timestamp) {
    if (!isValidUtcTimestamp(timestamp)) {
        return std::nullopt;
    }

    std::tm parsed{};
    std::istringstream stream(timestamp);
    stream >> std::get_time(&parsed, "%Y-%m-%dT%H:%M:%SZ");
    if (stream.fail()) {
        return std::nullopt;
    }

    return timegm(&parsed);
}

std::vector<std::string> split(const std::string &s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

}  // namespace

bool ValidationService::isSupportedMetric(const std::string &metricName) {
  return kSupportedMetrics.contains(metricName);
}

bool ValidationService::isSupportedStatistic(const std::string &statisticName) {
  return kSupportedStatistics.contains(statisticName);
}

std::vector<std::string> ValidationService::validateCreateReadingRequest(const Json::Value &payload) {
    std::vector<std::string> errors;

    if (!payload.isObject()) {
        errors.emplace_back("Request body must be a JSON object");
        return errors;
    }

    for (const auto &memberName : payload.getMemberNames()) {
        if (!kAllowedReadingFields.contains(memberName)) {
            errors.emplace_back("Unknown field '" + memberName + "'");
        }
    }

    if (!payload.isMember("sensorId") || !payload["sensorId"].isString() ||
        payload["sensorId"].asString().empty()) {
        errors.emplace_back("Field 'sensorId' is required and must be a non-empty string");
    }

    if (!payload.isMember("timestamp") || !payload["timestamp"].isString() ||
        !isValidUtcTimestamp(payload["timestamp"].asString())) {
        errors.emplace_back(
            "Field 'timestamp' is required and must be an ISO-8601 UTC timestamp");
    }

    bool hasAnyMetric = false;
    for (const auto &memberName : payload.getMemberNames()) {
        if (!isSupportedMetric(memberName)) {
            continue;
        }

        hasAnyMetric = true;
        if (!payload[memberName].isNumeric()) {
            errors.emplace_back("Field '" + memberName + "' must be numeric when provided");
        }
    }

    if (!hasAnyMetric) {
        errors.emplace_back(
            "At least one metric field must be provided: temperature, humidity, windSpeed");
    }

    return errors;
}

CreateReadingRequest ValidationService::buildCreateReadingRequest(const Json::Value &payload) {
    CreateReadingRequest request{
        .sensorId = payload["sensorId"].asString(),
        .timestamp = payload["timestamp"].asString(),
    };

    if (payload.isMember("temperature")) {
        request.temperature = payload["temperature"].asDouble();
    }

    if (payload.isMember("humidity")) {
        request.humidity = payload["humidity"].asDouble();
    }

    if (payload.isMember("windSpeed")) {
        request.windSpeed = payload["windSpeed"].asDouble();
    }

    return request;
}

MetricsQueryRequest ValidationService::buildMetricsQueryRequest(
    const drogon::HttpRequestPtr &request) {
    MetricsQueryRequest query{
        .sensorIds = split(request->getParameter("sensorId"), ','),
        .metrics = split(request->getParameter("metric"), ','),
        .statistic = request->getParameter("stat"),
        .from = std::nullopt,
        .to = std::nullopt,
    };

    if (const auto from = request->getParameter("from"); !from.empty()) {
        query.from = from;
    }

    if (const auto to = request->getParameter("to"); !to.empty()) {
        query.to = to;
    }

    return query;
}

std::vector<std::string> ValidationService::validateMetricsQueryRequest(
    const MetricsQueryRequest &request) {
    std::vector<std::string> errors;

    if (request.metrics.empty()) {
        errors.emplace_back("At least one metric parameter is required");
    }

    for (const auto &metric : request.metrics) {
        if (!isSupportedMetric(metric)) {
            errors.emplace_back("Unsupported metric '" + metric + "'");
        }
    }

    if (request.statistic.empty()) {
        errors.emplace_back("Query parameter 'stat' is required");
    } else if (!isSupportedStatistic(request.statistic)) {
        errors.emplace_back("Unsupported statistic '" + request.statistic + "'");
    }

    const bool hasFrom = request.from.has_value();
    const bool hasTo = request.to.has_value();
    if (hasFrom != hasTo) {
        errors.emplace_back("Query parameters 'from' and 'to' must be provided together");
    }

    if (hasFrom && hasTo) {
        const auto parsedFrom = parseUtcTimestamp(request.from.value());
        const auto parsedTo = parseUtcTimestamp(request.to.value());

        if (!parsedFrom.has_value()) {
            errors.emplace_back("Query parameter 'from' must be an ISO-8601 UTC timestamp");
        }
        if (!parsedTo.has_value()) {
            errors.emplace_back("Query parameter 'to' must be an ISO-8601 UTC timestamp");
        }

        if (parsedFrom.has_value() && parsedTo.has_value()) {
            if (parsedFrom.value() >= parsedTo.value()) {
                errors.emplace_back("Query parameter 'from' must be earlier than 'to'");
            } else {
                const auto durationSeconds = parsedTo.value() - parsedFrom.value();
                constexpr std::time_t kOneDaySeconds = 24 * 60 * 60;
                constexpr std::time_t kMaxRangeSeconds = 31 * kOneDaySeconds;

                if (durationSeconds < kOneDaySeconds) {
                    errors.emplace_back("Explicit date range must be at least 1 day");
                }
                if (durationSeconds > kMaxRangeSeconds) {
                    errors.emplace_back("Explicit date range must be at most 31 days");
                }
            }
        }
    }

    return errors;
}

std::string ValidationService::joinErrors(const std::vector<std::string> &errors) {
    std::ostringstream stream;
    for (size_t index = 0; index < errors.size(); ++index) {
        if (index > 0) {
            stream << "; ";
        }
        stream << errors[index];
    }

    return stream.str();
}

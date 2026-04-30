#include "ValidationService.hpp"

#include <regex>
#include <sstream>

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

bool isMetricField(const std::string &fieldName) {
    return fieldName == "temperature" || fieldName == "humidity" || fieldName == "windSpeed";
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
        if (!isMetricField(memberName)) {
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

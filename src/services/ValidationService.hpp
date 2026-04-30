#pragma once

#include "models/CreateReadingRequest.hpp"
#include "models/MetricsQueryRequest.hpp"

#include <drogon/HttpRequest.h>
#include <json/value.h>

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

class ValidationService {
  public:
    static bool isSupportedMetric(const std::string &metricName);
    static bool isSupportedStatistic(const std::string &statisticName);
    static std::vector<std::string> validateCreateReadingRequest(const Json::Value &payload);
    static CreateReadingRequest buildCreateReadingRequest(const Json::Value &payload);
    static MetricsQueryRequest buildMetricsQueryRequest(const drogon::HttpRequestPtr &request);
    static std::vector<std::string> validateMetricsQueryRequest(
        const MetricsQueryRequest &request);
    static std::string joinErrors(const std::vector<std::string> &errors);

  private:
    static const std::unordered_set<std::string> kSupportedMetrics;
    static const std::unordered_set<std::string> kSupportedStatistics;
    static const std::unordered_set<std::string> kAllowedReadingFields;
};

#pragma once

#include "models/CreateReadingRequest.hpp"

#include <json/value.h>

#include <string>
#include <unordered_set>
#include <vector>

class ValidationService {
  public:
    static bool isSupportedMetric(const std::string &metricName);
    static bool isSupportedStatistic(const std::string &statisticName);
    static std::vector<std::string> validateCreateReadingRequest(const Json::Value &payload);
    static CreateReadingRequest buildCreateReadingRequest(const Json::Value &payload);
    static std::string joinErrors(const std::vector<std::string> &errors);

  private:
    static const std::unordered_set<std::string> kSupportedMetrics;
    static const std::unordered_set<std::string> kSupportedStatistics;
    static const std::unordered_set<std::string> kAllowedReadingFields;
};

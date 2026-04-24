#include "ValidationService.hpp"

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

bool ValidationService::isSupportedMetric(const std::string &metricName) {
  return kSupportedMetrics.contains(metricName);
}

bool ValidationService::isSupportedStatistic(const std::string &statisticName) {
  return kSupportedStatistics.contains(statisticName);
}

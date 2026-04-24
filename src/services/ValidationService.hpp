#pragma once

#include <string>
#include <unordered_set>

class ValidationService {
  public:
    static bool isSupportedMetric(const std::string &metricName);
    static bool isSupportedStatistic(const std::string &statisticName);

  private:
    static const std::unordered_set<std::string> kSupportedMetrics;
    static const std::unordered_set<std::string> kSupportedStatistics;
};

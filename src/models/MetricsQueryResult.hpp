#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct SensorMetricsResult {
    std::string sensorId;
    std::unordered_map<std::string, double> metrics;
};

struct MetricsQueryResult {
    std::string statistic;
    std::optional<std::string> from;
    std::optional<std::string> to;
    std::vector<SensorMetricsResult> results;
};

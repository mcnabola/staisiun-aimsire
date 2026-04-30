#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct MetricsQueryRequest {
    std::vector<std::string> sensorIds;
    std::vector<std::string> metrics;
    std::string statistic;
    std::optional<std::string> from;
    std::optional<std::string> to;
};

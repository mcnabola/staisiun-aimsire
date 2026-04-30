#pragma once

#include <optional>
#include <string>

struct CreateReadingRequest {
    std::string sensorId;
    std::string timestamp;
    std::optional<double> temperature;
    std::optional<double> humidity;
    std::optional<double> windSpeed;

    [[nodiscard]] bool hasAnyMetric() const {
        return temperature.has_value() || humidity.has_value() || windSpeed.has_value();
    }
};

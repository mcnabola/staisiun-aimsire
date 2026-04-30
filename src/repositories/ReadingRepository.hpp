#pragma once

#include "models/CreateReadingRequest.hpp"
#include "models/MetricsQueryRequest.hpp"
#include "models/MetricsQueryResult.hpp"

#include <string>

class ReadingRepository {
  public:
    [[nodiscard]] std::string createReading(const CreateReadingRequest &request) const;
    [[nodiscard]] MetricsQueryResult queryMetrics(const MetricsQueryRequest &request) const;
};

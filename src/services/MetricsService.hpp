#pragma once

#include "models/MetricsQueryRequest.hpp"
#include "models/MetricsQueryResult.hpp"
#include "repositories/ReadingRepository.hpp"

#include <drogon/HttpRequest.h>

class MetricsService {
  public:
    explicit MetricsService(ReadingRepository repository = {});

    [[nodiscard]] MetricsQueryResult queryMetrics(const drogon::HttpRequestPtr &request) const;

  private:
    ReadingRepository repository_;
};

#include "MetricsService.hpp"

#include "services/ValidationService.hpp"

MetricsService::MetricsService(ReadingRepository repository)
    : repository_(std::move(repository)) {
}

MetricsQueryResult MetricsService::queryMetrics(const drogon::HttpRequestPtr &request) const {
    const auto query = ValidationService::buildMetricsQueryRequest(request);
    const auto errors = ValidationService::validateMetricsQueryRequest(query);
    if (!errors.empty()) {
        throw std::invalid_argument(ValidationService::joinErrors(errors));
    }

    return repository_.queryMetrics(query);
}

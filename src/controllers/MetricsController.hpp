#pragma once

#include <drogon/HttpController.h>

class MetricsController : public drogon::HttpController<MetricsController> {
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(MetricsController::getMetrics, "/api/v1/metrics", drogon::Get);
    METHOD_LIST_END

    void getMetrics(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback) const;
};

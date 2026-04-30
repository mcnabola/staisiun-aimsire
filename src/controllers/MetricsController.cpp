#include "MetricsController.hpp"

#include <json/json.h>

void MetricsController::getMetrics(
    const drogon::HttpRequestPtr &,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) const {
  Json::Value body;
  body["status"] = "not_implemented";
  body["message"] = "Metric aggregation todo.";

  auto response = drogon::HttpResponse::newHttpJsonResponse(body);
  response->setStatusCode(drogon::k501NotImplemented);
  callback(response);
}

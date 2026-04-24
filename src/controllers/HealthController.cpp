#include "HealthController.hpp"

#include <json/json.h>

void HealthController::getHealth(
    const drogon::HttpRequestPtr &,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) const {
  Json::Value body;
  body["status"] = "ok";

  auto response = drogon::HttpResponse::newHttpJsonResponse(body);
  callback(response);
}

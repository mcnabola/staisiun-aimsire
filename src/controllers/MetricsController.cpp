#include "MetricsController.hpp"

#include "models/MetricsQueryResult.hpp"
#include "services/MetricsService.hpp"

#include <json/json.h>

namespace {

Json::Value buildMetricsJson(const MetricsQueryResult &result) {
    Json::Value body;
    body["statistic"] = result.statistic;
    body["from"] = result.from.has_value() ? Json::Value(result.from.value()) : Json::nullValue;
    body["to"] = result.to.has_value() ? Json::Value(result.to.value()) : Json::nullValue;

    Json::Value results(Json::arrayValue);
    for (const auto &sensorResult : result.results) {
        Json::Value sensorJson;
        sensorJson["sensorId"] = sensorResult.sensorId;

        Json::Value metricsJson(Json::objectValue);
        for (const auto &[metric, value] : sensorResult.metrics) {
            metricsJson[metric] = value;
        }

        sensorJson["metrics"] = metricsJson;
        results.append(sensorJson);
    }

    body["results"] = results;
    return body;
}

}  // namespace

void MetricsController::getMetrics(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) const {
    try {
        MetricsService service;
        const auto result = service.queryMetrics(request);

        auto response = drogon::HttpResponse::newHttpJsonResponse(buildMetricsJson(result));
        response->setStatusCode(drogon::k200OK);
        callback(response);
    } catch (const std::invalid_argument &error) {
        Json::Value body;
        body["error"]["code"] = "VALIDATION_ERROR";
        body["error"]["message"] = error.what();

        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k400BadRequest);
        callback(response);
    } catch (const std::exception &error) {
        Json::Value body;
        body["error"]["code"] = "INTERNAL_ERROR";
        body["error"]["message"] = error.what();

        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k500InternalServerError);
        callback(response);
    }
}

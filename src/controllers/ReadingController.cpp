#include "ReadingController.hpp"

#include "services/ReadingService.hpp"

#include <drogon/orm/Exception.h>
#include <json/json.h>

void ReadingController::createReading(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) const {
    const auto json = request->getJsonObject();
    if (json == nullptr) {
        Json::Value body;
        body["error"]["code"] = "INVALID_JSON";
        body["error"]["message"] = "Request body must contain valid JSON";

        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k400BadRequest);
        callback(response);
        return;
    }

    try {
        ReadingService service;
        const auto readingId = service.createReading(*json);

        Json::Value body;
        body["status"] = "accepted";
        body["readingId"] = readingId;

        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k201Created);
        callback(response);
    } catch (const std::invalid_argument &error) {
        Json::Value body;
        body["error"]["code"] = "VALIDATION_ERROR";
        body["error"]["message"] = error.what();

        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k400BadRequest);
        callback(response);
    } catch (const drogon::orm::DrogonDbException &error) {
        Json::Value body;
        body["error"]["code"] = "DATABASE_ERROR";
        body["error"]["message"] = error.base().what();

        auto response = drogon::HttpResponse::newHttpJsonResponse(body);
        response->setStatusCode(drogon::k500InternalServerError);
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

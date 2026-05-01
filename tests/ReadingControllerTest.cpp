#include "controllers/ReadingController.hpp"

#include <drogon/HttpRequest.h>
#include <gtest/gtest.h>
#include <json/json.h>

TEST(ReadingControllerTest, ReturnsBadRequestWhenJsonIsMissing) {
    ReadingController controller;
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Post);
    request->setPath("/api/v1/readings");

    drogon::HttpResponsePtr response;
    controller.createReading(
        request, [&](const drogon::HttpResponsePtr &callbackResponse) {
            response = callbackResponse;
        });

    ASSERT_TRUE(response != nullptr);
    EXPECT_EQ(response->statusCode(), drogon::k400BadRequest);

    const auto json = response->getJsonObject();
    ASSERT_TRUE(json != nullptr);
    EXPECT_EQ((*json)["error"]["code"].asString(), "INVALID_JSON");
}

TEST(ReadingControllerTest, ReturnsBadRequestWhenValidationFails) {
    ReadingController controller;
    Json::Value payload;
    payload["sensorId"] = "sensor-1";
    payload["timestamp"] = "2026-04-27T09:30:00Z";

    auto request = drogon::HttpRequest::newHttpJsonRequest(payload);
    request->setMethod(drogon::Post);
    request->setPath("/api/v1/readings");

    drogon::HttpResponsePtr response;
    controller.createReading(
        request, [&](const drogon::HttpResponsePtr &callbackResponse) {
            response = callbackResponse;
        });

    ASSERT_TRUE(response != nullptr);
    EXPECT_EQ(response->statusCode(), drogon::k422UnprocessableEntity);

    const auto json = response->getJsonObject();
    ASSERT_TRUE(json != nullptr);
    EXPECT_EQ((*json)["error"]["code"].asString(), "VALIDATION_ERROR");
    EXPECT_NE((*json)["error"]["message"].asString().find("At least one metric field"),
              std::string::npos);
}

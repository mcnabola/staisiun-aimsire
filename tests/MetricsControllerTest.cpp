#include "controllers/MetricsController.hpp"

#include <drogon/HttpRequest.h>
#include <gtest/gtest.h>

TEST(MetricsControllerTest, ReturnsBadRequestWhenMetricIsMissing) {
    MetricsController controller;
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Get);
    request->setPath("/api/v1/metrics");
    request->setParameter("stat", "avg");

    drogon::HttpResponsePtr response;
    controller.getMetrics(request, [&](const drogon::HttpResponsePtr &callbackResponse) {
        response = callbackResponse;
    });

    ASSERT_TRUE(response != nullptr);
    EXPECT_EQ(response->statusCode(), drogon::k422UnprocessableEntity);

    const auto json = response->getJsonObject();
    ASSERT_TRUE(json != nullptr);
    EXPECT_EQ((*json)["error"]["code"].asString(), "VALIDATION_ERROR");
}

TEST(MetricsControllerTest, ReturnsBadRequestWhenDateRangeIsTooShort) {
    MetricsController controller;
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Get);
    request->setPath("/api/v1/metrics");
    request->setParameter("metric", "temperature");
    request->setParameter("stat", "avg");
    request->setParameter("from", "2026-04-01T00:00:00Z");
    request->setParameter("to", "2026-04-01T12:00:00Z");

    drogon::HttpResponsePtr response;
    controller.getMetrics(request, [&](const drogon::HttpResponsePtr &callbackResponse) {
        response = callbackResponse;
    });

    ASSERT_TRUE(response != nullptr);
    EXPECT_EQ(response->statusCode(), drogon::k422UnprocessableEntity);

    const auto json = response->getJsonObject();
    ASSERT_TRUE(json != nullptr);
    EXPECT_NE((*json)["error"]["message"].asString().find("at least 1 day"), std::string::npos);
}

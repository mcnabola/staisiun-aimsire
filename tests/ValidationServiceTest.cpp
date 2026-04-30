#include "services/ValidationService.hpp"

#include <gtest/gtest.h>
#include <json/json.h>

TEST(ValidationServiceTest, SupportsKnownMetrics) {
  EXPECT_TRUE(ValidationService::isSupportedMetric("temperature"));
  EXPECT_TRUE(ValidationService::isSupportedMetric("humidity"));
  EXPECT_TRUE(ValidationService::isSupportedMetric("windSpeed"));
}

TEST(ValidationServiceTest, RejectsUnknownMetrics) {
  EXPECT_FALSE(ValidationService::isSupportedMetric("pressure"));
  EXPECT_FALSE(ValidationService::isSupportedMetric("rainfall"));
}

TEST(ValidationServiceTest, SupportsKnownStatistics) {
  EXPECT_TRUE(ValidationService::isSupportedStatistic("avg"));
  EXPECT_TRUE(ValidationService::isSupportedStatistic("min"));
  EXPECT_TRUE(ValidationService::isSupportedStatistic("max"));
  EXPECT_TRUE(ValidationService::isSupportedStatistic("sum"));
}

TEST(ValidationServiceTest, RejectsUnknownStatistics) {
  EXPECT_FALSE(ValidationService::isSupportedStatistic("median"));
  EXPECT_FALSE(ValidationService::isSupportedStatistic("count"));
}

TEST(ValidationServiceTest, ValidatesCreateReadingRequestSuccessfully) {
  Json::Value payload;
  payload["sensorId"] = "sensor-1";
  payload["timestamp"] = "2026-04-25T10:15:00Z";
  payload["temperature"] = 18.7;

  EXPECT_TRUE(ValidationService::validateCreateReadingRequest(payload).empty());
}

TEST(ValidationServiceTest, RejectsCreateReadingRequestWithoutMetrics) {
  Json::Value payload;
  payload["sensorId"] = "sensor-1";
  payload["timestamp"] = "2026-04-25T10:15:00Z";

  const auto errors = ValidationService::validateCreateReadingRequest(payload);
  ASSERT_FALSE(errors.empty());
  EXPECT_NE(
      ValidationService::joinErrors(errors).find("At least one metric field"),
      std::string::npos);
}

TEST(ValidationServiceTest, RejectsCreateReadingRequestWithUnknownFields) {
  Json::Value payload;
  payload["sensorId"] = "sensor-1";
  payload["timestamp"] = "2026-04-25T10:15:00Z";
  payload["temperature"] = 18.7;
  payload["pressure"] = 1001;

  const auto errors = ValidationService::validateCreateReadingRequest(payload);
  ASSERT_FALSE(errors.empty());
  EXPECT_NE(
      ValidationService::joinErrors(errors).find("Unknown field 'pressure'"),
      std::string::npos);
}

TEST(ValidationServiceTest, RejectsCreateReadingRequestWithInvalidTimestamp) {
  Json::Value payload;
  payload["sensorId"] = "sensor-1";
  payload["timestamp"] = "2026-04-25 10:15:00";
  payload["humidity"] = 56.2;

  const auto errors = ValidationService::validateCreateReadingRequest(payload);
  ASSERT_FALSE(errors.empty());
  EXPECT_NE(ValidationService::joinErrors(errors).find("Field 'timestamp'"),
            std::string::npos);
}

TEST(ValidationServiceTest,
     BuildMetricsQueryRequestHandlesCommaSeparatedValues) {
  auto request = drogon::HttpRequest::newHttpRequest();
  request->setMethod(drogon::Get);
  request->setPath("/api/v1/metrics");
  request->setParameter("sensorId", "sensor-1,sensor-2");
  request->setParameter("metric", "temperature,humidity");
  request->setParameter("stat", "avg");
  request->setParameter("from", "2026-04-01T00:00:00Z");
  request->setParameter("to", "2026-04-02T00:00:00Z");

  const auto query = ValidationService::buildMetricsQueryRequest(request);

  EXPECT_EQ(query.sensorIds,
            (std::vector<std::string>{"sensor-1", "sensor-2"}));
  EXPECT_EQ(query.metrics,
            (std::vector<std::string>{"temperature", "humidity"}));
  EXPECT_EQ(query.statistic, "avg");
}

TEST(ValidationServiceTest,
     BuildMetricsQueryRequestHandlesCommaSeparatedValuesWithSpaces) {
  auto request = drogon::HttpRequest::newHttpRequest();
  request->setMethod(drogon::Get);
  request->setPath("/api/v1/metrics");
  request->setParameter("sensorId", "sensor-1, sensor-2 ");
  request->setParameter("metric", " temperature , humidity ");
  request->setParameter("stat", "avg");
  request->setParameter("from", "2026-04-01T00:00:00Z");
  request->setParameter("to", "2026-04-02T00:00:00Z");

  const auto query = ValidationService::buildMetricsQueryRequest(request);

  EXPECT_EQ(query.sensorIds,
            (std::vector<std::string>{"sensor-1", " sensor-2 "}));
  EXPECT_EQ(query.metrics,
            (std::vector<std::string>{" temperature ", " humidity "}));
  EXPECT_EQ(query.statistic, "avg");
}

TEST(ValidationServiceTest, ValidatesMetricsQueryRequestSuccessfully) {
  MetricsQueryRequest request{
      .sensorIds = {"sensor-1"},
      .metrics = {"temperature", "humidity"},
      .statistic = "avg",
      .from = "2026-04-01T00:00:00Z",
      .to = "2026-04-02T00:00:00Z",
  };

  EXPECT_TRUE(ValidationService::validateMetricsQueryRequest(request).empty());
}

TEST(ValidationServiceTest, RejectsMetricsQueryWithSpacesInParams) {
  MetricsQueryRequest request{
      .sensorIds = {"sensor-1", " sensor-2 "},
      .metrics = {" temperature ", " humidity "},
      .statistic = "avg",
      .from = "2026-04-01T00:00:00Z",
      .to = "2026-04-02T00:00:00Z",
  };

  const auto errors = ValidationService::validateMetricsQueryRequest(request);
  ASSERT_FALSE(errors.empty());
  EXPECT_NE(ValidationService::joinErrors(errors).find(
                "Unsupported metric ' temperature '"),
            std::string::npos);
  EXPECT_NE(ValidationService::joinErrors(errors).find(
                "Unsupported metric ' humidity '"),
            std::string::npos);
}

TEST(ValidationServiceTest, RejectsMetricsQueryRequestWithUnsupportedMetric) {
  MetricsQueryRequest request{
      .sensorIds = {"sensor-1"},
      .metrics = {"pressure"},
      .statistic = "avg",
  };

  const auto errors = ValidationService::validateMetricsQueryRequest(request);
  ASSERT_FALSE(errors.empty());
  EXPECT_NE(ValidationService::joinErrors(errors).find(
                "Unsupported metric 'pressure'"),
            std::string::npos);
}

TEST(ValidationServiceTest, RejectsMetricsQueryRequestWhenDateRangeTooLarge) {
  MetricsQueryRequest request{
      .metrics = {"temperature"},
      .statistic = "avg",
      .from = "2026-04-01T00:00:00Z",
      .to = "2026-05-10T00:00:00Z",
  };

  const auto errors = ValidationService::validateMetricsQueryRequest(request);
  ASSERT_FALSE(errors.empty());
  EXPECT_NE(ValidationService::joinErrors(errors).find("at most 31 days"),
            std::string::npos);
}

TEST(ValidationServiceTest, RejectsMetricsQueryRequestWhenDateRangeTooShort) {
  MetricsQueryRequest request{
      .metrics = {"temperature"},
      .statistic = "avg",
      .from = "2026-04-01T00:00:00Z",
      .to = "2026-04-01T00:20:00Z",
  };

  const auto errors = ValidationService::validateMetricsQueryRequest(request);
  ASSERT_FALSE(errors.empty());
  EXPECT_NE(ValidationService::joinErrors(errors).find("at least 1 day"),
            std::string::npos);
}

TEST(ValidationServiceTest, MetricsQueryRequestValidWhenNoTimeRange) {
  MetricsQueryRequest request{
      .sensorIds = {"sensor-1"},
      .metrics = {"temperature"},
      .statistic = "avg",
  };

  EXPECT_TRUE(ValidationService::validateMetricsQueryRequest(request).empty());
}

TEST(ValidationServiceTest, RejectsMetricsQueryRequestWhenNoSensorSpecified) {
  MetricsQueryRequest request{
      .metrics = {"temperature"},
      .statistic = "avg",
  };

  const auto errors = ValidationService::validateMetricsQueryRequest(request);
  ASSERT_FALSE(errors.empty());
  EXPECT_NE(ValidationService::joinErrors(errors).find(
                "At least one sensorId parameter is required"),
            std::string::npos);
}

TEST(ValidationServiceTest, RejectsMetricsQueryRequestWhenInvalidTimestamp) {
  MetricsQueryRequest request{
      .metrics = {"temperature"},
      .statistic = "avg",
      .from = "15/04/2026 07:30 PM",
      .to = "April 16th 2026 07:30 PM",
  };

  const auto errors = ValidationService::validateMetricsQueryRequest(request);
  ASSERT_FALSE(errors.empty());
  EXPECT_NE(ValidationService::joinErrors(errors).find(
                "Query parameter 'from' must be an ISO-8601 UTC timestamp"),
            std::string::npos);
  EXPECT_NE(ValidationService::joinErrors(errors).find(
                "Query parameter 'to' must be an ISO-8601 UTC timestamp"),
            std::string::npos);
}

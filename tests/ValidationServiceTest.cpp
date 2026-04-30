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
  EXPECT_NE(ValidationService::joinErrors(errors).find("At least one metric field"),
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
  EXPECT_NE(ValidationService::joinErrors(errors).find("Unknown field 'pressure'"),
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

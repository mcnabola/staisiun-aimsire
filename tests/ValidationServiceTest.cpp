#include "services/ValidationService.hpp"

#include <gtest/gtest.h>

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

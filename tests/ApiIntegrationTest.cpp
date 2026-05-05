#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/DbConfig.h>
#include <gtest/gtest.h>
#include <json/json.h>

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include "services/DatabaseSchemaService.hpp"

namespace {

struct IntegrationConfig {
    std::string dbHost = "127.0.0.1";
    unsigned short dbPort = 5432;
    std::string dbName = "weather_data_test";
    std::string dbUser = "postgres";
    std::string dbPassword = "postgres";
    unsigned short apiPort = 18080;
};

struct IntegrationState {
    bool initialized = false;
    bool ready = false;
    std::string skipReason;
    std::thread serverThread;
};

IntegrationState gState;

std::string getenvOrDefault(const char *name, const char *defaultValue) {
    if (const auto *value = std::getenv(name)) {
        return value;
    }
    return defaultValue;
}

unsigned short getenvU16OrDefault(const char *name, unsigned short defaultValue) {
    if (const auto *value = std::getenv(name)) {
        return static_cast<unsigned short>(std::stoi(value));
    }
    return defaultValue;
}

bool shouldRunIntegrationTests() {
    // turn on/off integration tests now
    return true;
}

IntegrationConfig loadConfig() {
    return {
        .dbHost = getenvOrDefault("WEATHER_DATA_TEST_DB_HOST", "127.0.0.1"),
        .dbPort = getenvU16OrDefault("WEATHER_DATA_TEST_DB_PORT", 5432),
        .dbName = getenvOrDefault("WEATHER_DATA_TEST_DB_NAME", "weather_data_test"),
        .dbUser = getenvOrDefault("WEATHER_DATA_TEST_DB_USER", "postgres"),
        .dbPassword = getenvOrDefault("WEATHER_DATA_TEST_DB_PASSWORD", "postgres"),
        .apiPort = getenvU16OrDefault("WEATHER_DATA_TEST_API_PORT", 18080),
    };
}

void applySchema(const drogon::orm::DbClientPtr &dbClient) {
    std::string schemaPath = WEATHER_DATA_SOURCE_DIR "/db/migrations/001_initial_schema.sql";
    weather_data::services::DatabaseSchemaService::ensureSchema(dbClient, schemaPath);
}

void cleanDatabase(const drogon::orm::DbClientPtr &dbClient) {
    dbClient->execSqlSync("TRUNCATE TABLE readings, sensors RESTART IDENTITY CASCADE");
}

void initializeIntegrationState() {
    if (gState.initialized) {
        return;
    }
    gState.initialized = true;

    if (!shouldRunIntegrationTests()) {
        gState.skipReason =
            "Set WEATHER_DATA_RUN_DB_TESTS=1 to enable real database integration tests";
        return;
    }

    const auto config = loadConfig();

    try {
        drogon::app().setThreadNum(1);
        drogon::app().setLogLevel(trantor::Logger::kWarn);
        drogon::app().addListener("127.0.0.1", config.apiPort);
        drogon::app().addDbClient(drogon::orm::PostgresConfig{
            .host = config.dbHost,
            .port = config.dbPort,
            .databaseName = config.dbName,
            .username = config.dbUser,
            .password = config.dbPassword,
            .connectionNumber = 1,
            .name = "default",
            .isFast = false,
            .characterSet = "",
            .timeout = 10,
            .autoBatch = false,
            .connectOptions = {},
        });

        gState.serverThread = std::thread([] {
            // start the weather data REST server
            drogon::app().run();
        });

        for (int attempt = 0; attempt < 50 && !drogon::app().isRunning(); ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!drogon::app().isRunning()) {
            gState.skipReason = "Drogon app did not start in time";
            return;
        }

        for (int attempt = 0; attempt < 50 && !drogon::app().areAllDbClientsAvailable();
             ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!drogon::app().areAllDbClientsAvailable()) {
            gState.skipReason =
                "Database client is not available. Check PostgreSQL and test DB settings.";
            drogon::app().quit();
            if (gState.serverThread.joinable()) {
                gState.serverThread.join();
            }
            return;
        }

        applySchema(drogon::app().getDbClient());
        gState.ready = true;
    } catch (const std::exception &error) {
        gState.skipReason = error.what();
        if (drogon::app().isRunning()) {
            drogon::app().quit();
        }
        if (gState.serverThread.joinable()) {
            gState.serverThread.join();
        }
    }
}

class ApiIntegrationTest : public ::testing::Test {
  protected:
    static void SetUpTestSuite() {
        initializeIntegrationState();
    }

    static void TearDownTestSuite() {
        if (gState.ready && drogon::app().isRunning()) {
            drogon::app().quit();
        }
        if (gState.serverThread.joinable()) {
            gState.serverThread.join();
        }
    }

    void SetUp() override {
        if (!gState.ready) {
            GTEST_SKIP() << gState.skipReason;
        }
        cleanDatabase(drogon::app().getDbClient());
    }

    // Database Helpers
    void createSensor(const std::string &externalId) {
        drogon::app().getDbClient()->execSqlSync(
            "INSERT INTO sensors (external_id) VALUES ($1)", externalId);
    }

    void addReading(const std::string &externalId, const std::string &timestamp,
                    double temp, double humidity, std::optional<double> wind = std::nullopt) {
        if (wind.has_value()) {
            drogon::app().getDbClient()->execSqlSync(R"(
                INSERT INTO readings (sensor_id, recorded_at, temperature, humidity, wind_speed)
                SELECT id, $2::timestamptz, $3, $4, $5 FROM sensors WHERE external_id = $1
            )",
                                                      externalId, timestamp, temp, humidity, wind.value());
        } else {
            drogon::app().getDbClient()->execSqlSync(R"(
                INSERT INTO readings (sensor_id, recorded_at, temperature, humidity, wind_speed)
                SELECT id, $2::timestamptz, $3, $4, $5 FROM sensors WHERE external_id = $1
            )",
                                                      externalId, timestamp, temp, humidity, nullptr);
        }
    }

    // API Helpers
    drogon::HttpResponsePtr get(const std::string &path) {
        const auto config = loadConfig();
        auto client =
            drogon::HttpClient::newHttpClient("http://127.0.0.1:" + std::to_string(config.apiPort));
        auto request = drogon::HttpRequest::newHttpRequest();
        request->setMethod(drogon::Get);
        request->setPath(path);
        const auto [result, response] = client->sendRequest(request, 5.0);
        EXPECT_EQ(result, drogon::ReqResult::Ok);
        return response;
    }

    drogon::HttpResponsePtr post(const std::string &path, const Json::Value &payload) {
        const auto config = loadConfig();
        auto client =
            drogon::HttpClient::newHttpClient("http://127.0.0.1:" + std::to_string(config.apiPort));
        auto request = drogon::HttpRequest::newHttpJsonRequest(payload);
        request->setMethod(drogon::Post);
        request->setPath(path);
        const auto [result, response] = client->sendRequest(request, 5.0);
        EXPECT_EQ(result, drogon::ReqResult::Ok);
        return response;
    }

    // Verification Helpers
    void expectStatus(const drogon::HttpResponsePtr &resp, drogon::HttpStatusCode code) {
        ASSERT_TRUE(resp != nullptr);
        EXPECT_EQ(resp->statusCode(), code);
    }

    void expectMetric(const Json::Value &results, const std::string &sensorId,
                      const std::string &metric, double expectedValue) {
        bool found = false;
        for (const auto &item : results) {
            if (item["sensorId"].asString() == sensorId) {
                found = true;
                EXPECT_DOUBLE_EQ(item["metrics"][metric].asDouble(), expectedValue);
                break;
            }
        }
        EXPECT_TRUE(found) << "Sensor " << sensorId << " not found in results";
    }
};

TEST_F(ApiIntegrationTest, PostReadingsPersistsRowInDatabase) {
    Json::Value payload;
    payload["sensorId"] = "sensor-1";
    payload["timestamp"] = "2026-04-27T10:15:00Z";
    payload["temperature"] = 18.7;
    payload["humidity"] = 56.2;

    auto response = post("/api/v1/readings", payload);
    expectStatus(response, drogon::k201Created);

    const auto json = response->getJsonObject();
    EXPECT_EQ((*json)["status"].asString(), "accepted");
    EXPECT_FALSE((*json)["readingId"].asString().empty());

    const auto dbResult = drogon::app().getDbClient()->execSqlSync(R"(
        SELECT s.external_id, r.temperature, r.humidity, r.wind_speed
        FROM readings r
        JOIN sensors s ON s.id = r.sensor_id
    )");

    ASSERT_EQ(dbResult.size(), 1);
    EXPECT_EQ(dbResult[0]["external_id"].as<std::string>(), "sensor-1");
    EXPECT_DOUBLE_EQ(dbResult[0]["temperature"].as<double>(), 18.7);
    EXPECT_DOUBLE_EQ(dbResult[0]["humidity"].as<double>(), 56.2);
    EXPECT_TRUE(dbResult[0]["wind_speed"].isNull());
}

TEST_F(ApiIntegrationTest, PostReadingsReturnsValidReadingId) {
    Json::Value payload;
    payload["sensorId"] = "sensor-1";
    payload["timestamp"] = "2026-04-27T10:15:00Z";
    payload["temperature"] = 22.5;

    auto response = post("/api/v1/readings", payload);
    expectStatus(response, drogon::k201Created);

    const auto json = response->getJsonObject();
    const std::string readingId = (*json)["readingId"].asString();
    ASSERT_FALSE(readingId.empty());

    // Verify the reading exists in the database using the returned readingId
    const auto dbResult = drogon::app().getDbClient()->execSqlSync(
        "SELECT id::text FROM readings WHERE id = $1::uuid", readingId);

    ASSERT_EQ(dbResult.size(), 1);
    EXPECT_EQ(dbResult[0]["id"].as<std::string>(), readingId);
}

TEST_F(ApiIntegrationTest, PostReadingsIsDuplicateAndNotIdempotent) {
    Json::Value payload;
    payload["sensorId"] = "sensor-1";
    payload["timestamp"] = "2026-04-27T10:15:00Z";
    payload["temperature"] = 22.5;

    // Send first request
    auto response1 = post("/api/v1/readings", payload);
    expectStatus(response1, drogon::k201Created);
    const std::string id1 = (*response1->getJsonObject())["readingId"].asString();

    // Send identical second request
    auto response2 = post("/api/v1/readings", payload);
    expectStatus(response2, drogon::k201Created);
    const std::string id2 = (*response2->getJsonObject())["readingId"].asString();

    // Verify they are different readings
    EXPECT_NE(id1, id2);

    // Verify both exist in the database
    const auto dbResult = drogon::app().getDbClient()->execSqlSync(
        "SELECT COUNT(*) as count FROM readings WHERE sensor_id = "
        "(SELECT id FROM sensors WHERE external_id = 'sensor-1')");
    
    ASSERT_EQ(dbResult.size(), 1);
    EXPECT_EQ(dbResult[0]["count"].as<long long>(), 2);
}

TEST_F(ApiIntegrationTest, PostReadingsRejectsInvalidPayloadWithoutPersistingRows) {
    Json::Value payload;
    payload["sensorId"] = "sensor-1";
    payload["timestamp"] = "2026-04-27T10:15:00Z";

    auto response = post("/api/v1/readings", payload);
    expectStatus(response, drogon::k422UnprocessableEntity);

    const auto json = response->getJsonObject();
    EXPECT_EQ((*json)["error"]["code"].asString(), "VALIDATION_ERROR");

    const auto counts = drogon::app().getDbClient()->execSqlSync(R"(
        SELECT
            (SELECT COUNT(*) FROM sensors) AS sensor_count,
            (SELECT COUNT(*) FROM readings) AS reading_count
    )");

    ASSERT_EQ(counts.size(), 1);
    EXPECT_EQ(counts[0]["sensor_count"].as<long long>(), 0);
    EXPECT_EQ(counts[0]["reading_count"].as<long long>(), 0);
}

TEST_F(ApiIntegrationTest, GetMetricsReturnsAggregatedAverageForDateRange) {
    createSensor("sensor-1");
    createSensor("sensor-2");
    addReading("sensor-1", "2026-04-01T00:00:00Z", 10.0, 50.0);
    addReading("sensor-1", "2026-04-02T00:00:00Z", 20.0, 70.0);
    addReading("sensor-1", "2026-04-01T12:00:00Z", 15.0, 60.0);
    addReading("sensor-2", "2026-04-02T00:00:00Z", 30.0, 80.0);

    auto response = get("/api/v1/metrics?sensorId=sensor-1,sensor-2&metric=temperature,humidity&stat=avg"
                        "&from=2026-04-01T00:00:00Z&to=2026-04-02T00:00:00Z");
    expectStatus(response, drogon::k200OK);

    const auto json = response->getJsonObject();
    EXPECT_EQ((*json)["statistic"].asString(), "avg");
    ASSERT_EQ((*json)["results"].size(), 2);
    expectMetric((*json)["results"], "sensor-1", "temperature", 15.0);
    expectMetric((*json)["results"], "sensor-1", "humidity", 60.0);
    expectMetric((*json)["results"], "sensor-2", "temperature", 30.0);
    expectMetric((*json)["results"], "sensor-2", "humidity", 80.0);
}

TEST_F(ApiIntegrationTest, GetMetricsReturnsAverageForAllSensorsWhenNoSensorId) {
    createSensor("sensor-1");
    createSensor("sensor-2");
    addReading("sensor-1", "2026-04-01T00:00:00Z", 10.0, 50.0);
    addReading("sensor-1", "2026-04-02T00:00:00Z", 20.0, 70.0);
    addReading("sensor-1", "2026-04-01T12:00:00Z", 15.0, 60.0);
    addReading("sensor-2", "2026-04-02T00:00:00Z", 30.0, 80.0);

    auto response = get("/api/v1/metrics?metric=temperature,humidity&stat=avg"
                        "&from=2026-04-01T00:00:00Z&to=2026-04-02T00:00:00Z");
    expectStatus(response, drogon::k200OK);

    const auto json = response->getJsonObject();
    EXPECT_EQ((*json)["statistic"].asString(), "avg");
    ASSERT_EQ((*json)["results"].size(), 2);
    expectMetric((*json)["results"], "sensor-1", "temperature", 15.0);
    expectMetric((*json)["results"], "sensor-1", "humidity", 60.0);
    expectMetric((*json)["results"], "sensor-2", "temperature", 30.0);
    expectMetric((*json)["results"], "sensor-2", "humidity", 80.0);
}

TEST_F(ApiIntegrationTest, GetMetricsReturnsAggregatedSumForDateRange) {
    createSensor("sensor-1");
    addReading("sensor-1", "2026-04-01T00:00:00Z", 10.0, 50.0);
    addReading("sensor-1", "2026-04-02T00:00:00Z", 20.0, 70.0);
    addReading("sensor-1", "2026-04-01T12:00:00Z", 15.0, 60.0);

    auto response = get("/api/v1/metrics?sensorId=sensor-1&metric=temperature,humidity&stat=sum"
                        "&from=2026-04-01T00:00:00Z&to=2026-04-02T00:00:00Z");
    expectStatus(response, drogon::k200OK);

    const auto json = response->getJsonObject();
    EXPECT_EQ((*json)["statistic"].asString(), "sum");
    ASSERT_EQ((*json)["results"].size(), 1);
    expectMetric((*json)["results"], "sensor-1", "temperature", 45.0);
    expectMetric((*json)["results"], "sensor-1", "humidity", 180.0);
}

TEST_F(ApiIntegrationTest, GetMetricsReturnsAggregatedMinForDateRange) {
    createSensor("sensor-1");
    addReading("sensor-1", "2026-04-01T00:00:00Z", 10.0, 50.0);
    addReading("sensor-1", "2026-04-02T00:00:00Z", 20.0, 70.0);
    addReading("sensor-1", "2026-04-01T12:00:00Z", 15.0, 60.0);

    auto response = get("/api/v1/metrics?metric=temperature,humidity&stat=min"
                        "&from=2026-04-01T00:00:00Z&to=2026-04-02T00:00:00Z");
    expectStatus(response, drogon::k200OK);

    const auto json = response->getJsonObject();
    EXPECT_EQ((*json)["statistic"].asString(), "min");
    ASSERT_EQ((*json)["results"].size(), 1);
    expectMetric((*json)["results"], "sensor-1", "temperature", 10.0);
    expectMetric((*json)["results"], "sensor-1", "humidity", 50.0);
}

TEST_F(ApiIntegrationTest, GetMetricsReturnsMaxForDateRange) {
    createSensor("sensor-1");
    addReading("sensor-1", "2026-04-01T00:00:00Z", 20.0, 50.0);
    addReading("sensor-1", "2026-04-02T00:00:00Z", 10.0, 70.0);
    addReading("sensor-1", "2026-04-01T12:00:00Z", 15.0, 60.0);

    auto response = get("/api/v1/metrics?metric=temperature,humidity&stat=max"
                        "&from=2026-04-01T00:00:00Z&to=2026-04-02T00:00:00Z");
    expectStatus(response, drogon::k200OK);

    const auto json = response->getJsonObject();
    EXPECT_EQ((*json)["statistic"].asString(), "max");
    ASSERT_EQ((*json)["results"].size(), 1);
    expectMetric((*json)["results"], "sensor-1", "temperature", 20.0);
    expectMetric((*json)["results"], "sensor-1", "humidity", 70.0);
}

TEST_F(ApiIntegrationTest, GetMetricsUses24HourWindowWhenDateRangeIsOmitted) {
    createSensor("sensor-1");
    // Latest is 2026-04-03T12:00:00Z
    // 24h window starts at 2026-04-02T12:00:00Z
    addReading("sensor-1", "2026-04-02T11:00:00Z", 10.0, 50.0);  // Out (too old)
    addReading("sensor-1", "2026-04-02T13:00:00Z", 20.0, 60.0);  // In
    addReading("sensor-1", "2026-04-03T12:00:00Z", 30.0, 70.0);  // In (Latest)

    auto response = get("/api/v1/metrics?metric=temperature&stat=avg&sensorId=sensor-1");
    expectStatus(response, drogon::k200OK);

    const auto json = response->getJsonObject();
    EXPECT_EQ((*json)["from"].asString(), "2026-04-02T12:00:00Z");
    EXPECT_EQ((*json)["to"].asString(), "2026-04-03T12:00:00Z");

    ASSERT_EQ((*json)["results"].size(), 1);
    // Average of 20 and 30 is 25
    expectMetric((*json)["results"], "sensor-1", "temperature", 25.0);
}

TEST_F(ApiIntegrationTest, GetMetricsIncludesMultipleSensorsIn24HourWindow) {
    createSensor("sensor-old");
    createSensor("sensor-new");

    // sensor-new's latest is at 11:00
    // 24h window is [yesterday 11:00, today 11:00]
    addReading("sensor-new", "2026-04-01T11:00:00Z", 20.0, 60.0);

    // sensor-old's reading is at 10:00 (1 hour before sensor-new's latest)
    // This IS within the 24h window!
    addReading("sensor-old", "2026-04-01T10:00:00Z", 10.0, 50.0);

    // An even older reading for sensor-old (25 hours before sensor-new's latest)
    addReading("sensor-old", "2026-03-31T10:00:00Z", 5.0, 40.0);  // Out

    auto response = get("/api/v1/metrics?metric=temperature&stat=avg&sensorId=sensor-old,sensor-new");
    expectStatus(response, drogon::k200OK);

    const auto json = response->getJsonObject();
    EXPECT_EQ((*json)["from"].asString(), "2026-03-31T11:00:00Z");
    EXPECT_EQ((*json)["to"].asString(), "2026-04-01T11:00:00Z");

    ASSERT_EQ((*json)["results"].size(), 2);
    expectMetric((*json)["results"], "sensor-new", "temperature", 20.0);
    expectMetric((*json)["results"], "sensor-old", "temperature", 10.0);
}

TEST_F(ApiIntegrationTest, GetMetricsReturnsCorrectlyWhenNoDataFound) {
    createSensor("sensor-1");
    addReading("sensor-1", "2026-04-01T00:00:00Z", 10.0, 50.0);
    addReading("sensor-1", "2026-04-03T00:00:00Z", 40.0, 90.0);

    auto response = get("/api/v1/metrics?metric=temperature&stat=max&sensorId=sensor-2");
    expectStatus(response, drogon::k200OK);

    const auto json = response->getJsonObject();
    EXPECT_EQ((*json)["statistic"].asString(), "max");
    ASSERT_EQ((*json)["results"].size(), 0);
}

}  // namespace

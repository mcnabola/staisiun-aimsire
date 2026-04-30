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
    return true;
    //TODO: document
    // if (const auto *value = std::getenv("WEATHER_DATA_RUN_DB_TESTS")) {
    //     return std::string(value) == "1";
    // }
    // return false;
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
    dbClient->execSqlSync(R"(CREATE EXTENSION IF NOT EXISTS "pgcrypto")");
    dbClient->execSqlSync(R"(
        CREATE TABLE IF NOT EXISTS sensors (
            id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
            external_id TEXT NOT NULL UNIQUE,
            created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
        )
    )");
    dbClient->execSqlSync(R"(
        CREATE TABLE IF NOT EXISTS readings (
            id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
            sensor_id UUID NOT NULL REFERENCES sensors(id),
            recorded_at TIMESTAMPTZ NOT NULL,
            temperature DOUBLE PRECISION NULL,
            humidity DOUBLE PRECISION NULL,
            wind_speed DOUBLE PRECISION NULL,
            created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
        )
    )");
    dbClient->execSqlSync(
        "CREATE INDEX IF NOT EXISTS idx_readings_sensor_recorded_at "
        "ON readings(sensor_id, recorded_at)");
    dbClient->execSqlSync(
        "CREATE INDEX IF NOT EXISTS idx_readings_recorded_at ON readings(recorded_at)");
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
};

TEST_F(ApiIntegrationTest, PostReadingsPersistsRowInDatabase) {
    const auto config = loadConfig();
    auto client =
        drogon::HttpClient::newHttpClient("http://127.0.0.1:" + std::to_string(config.apiPort));

    Json::Value payload;
    payload["sensorId"] = "sensor-1";
    payload["timestamp"] = "2026-04-27T10:15:00Z";
    payload["temperature"] = 18.7;
    payload["humidity"] = 56.2;

    auto request = drogon::HttpRequest::newHttpJsonRequest(payload);
    request->setMethod(drogon::Post);
    request->setPath("/api/v1/readings");

    const auto [result, response] = client->sendRequest(request, 5.0);
    ASSERT_EQ(result, drogon::ReqResult::Ok);
    ASSERT_TRUE(response != nullptr);
    EXPECT_EQ(response->statusCode(), drogon::k201Created);

    const auto json = response->getJsonObject();
    ASSERT_TRUE(json != nullptr);
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

TEST_F(ApiIntegrationTest, PostReadingsRejectsInvalidPayloadWithoutPersistingRows) {
    const auto config = loadConfig();
    auto client =
        drogon::HttpClient::newHttpClient("http://127.0.0.1:" + std::to_string(config.apiPort));

    Json::Value payload;
    payload["sensorId"] = "sensor-1";
    payload["timestamp"] = "2026-04-27T10:15:00Z";

    auto request = drogon::HttpRequest::newHttpJsonRequest(payload);
    request->setMethod(drogon::Post);
    request->setPath("/api/v1/readings");

    const auto [result, response] = client->sendRequest(request, 5.0);
    ASSERT_EQ(result, drogon::ReqResult::Ok);
    ASSERT_TRUE(response != nullptr);
    EXPECT_EQ(response->statusCode(), drogon::k400BadRequest);

    const auto json = response->getJsonObject();
    ASSERT_TRUE(json != nullptr);
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
    auto dbClient = drogon::app().getDbClient();
    dbClient->execSqlSync(
        "INSERT INTO sensors (external_id) VALUES ($1), ($2)",
        "sensor-1",
        "sensor-2");
    dbClient->execSqlSync(R"(
        INSERT INTO readings (sensor_id, recorded_at, temperature, humidity, wind_speed)
        SELECT id, $2::timestamptz, $3, $4, $5 FROM sensors WHERE external_id = $1
    )",
                          "sensor-1",
                          "2026-04-01T00:00:00Z",
                          10.0,
                          50.0,
                          nullptr);
    dbClient->execSqlSync(R"(
        INSERT INTO readings (sensor_id, recorded_at, temperature, humidity, wind_speed)
        SELECT id, $2::timestamptz, $3, $4, $5 FROM sensors WHERE external_id = $1
    )",
                          "sensor-1",
                          "2026-04-02T00:00:00Z",
                          20.0,
                          70.0,
                          nullptr);
    dbClient->execSqlSync(R"(
        INSERT INTO readings (sensor_id, recorded_at, temperature, humidity, wind_speed)
        SELECT id, $2::timestamptz, $3, $4, $5 FROM sensors WHERE external_id = $1
    )",
                          "sensor-1",
                          "2026-04-01T12:00:00Z",
                          15.0,
                          60.0,
                          nullptr);
    dbClient->execSqlSync(R"(
        INSERT INTO readings (sensor_id, recorded_at, temperature, humidity, wind_speed)
        SELECT id, $2::timestamptz, $3, $4, $5 FROM sensors WHERE external_id = $1
    )",
                          "sensor-2",
                          "2026-04-02T00:00:00Z",
                          30.0,
                          80.0,
                          nullptr);

    const auto config = loadConfig();
    auto client =
        drogon::HttpClient::newHttpClient("http://127.0.0.1:" + std::to_string(config.apiPort));

    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Get);
    request->setPath(
        "/api/v1/metrics?sensorId=sensor-1,sensor-2&metric=temperature,humidity&stat=avg&from=2026-04-01T00:00:00Z"
        "&to=2026-04-02T00:00:00Z");

    const auto [result, response] = client->sendRequest(request, 5.0);
    ASSERT_EQ(result, drogon::ReqResult::Ok);
    ASSERT_TRUE(response != nullptr);
    EXPECT_EQ(response->statusCode(), drogon::k200OK);

    const auto json = response->getJsonObject();
    ASSERT_TRUE(json != nullptr);
    EXPECT_EQ((*json)["statistic"].asString(), "avg");
    ASSERT_EQ((*json)["results"].size(), 2);
    EXPECT_EQ((*json)["results"][0]["sensorId"].asString(), "sensor-1");
    EXPECT_DOUBLE_EQ((*json)["results"][0]["metrics"]["temperature"].asDouble(), 15.0);
    EXPECT_DOUBLE_EQ((*json)["results"][0]["metrics"]["humidity"].asDouble(), 60.0);
    EXPECT_EQ((*json)["results"][1]["sensorId"].asString(), "sensor-2");
    EXPECT_DOUBLE_EQ((*json)["results"][1]["metrics"]["temperature"].asDouble(), 30.0);
    EXPECT_DOUBLE_EQ((*json)["results"][1]["metrics"]["humidity"].asDouble(), 80.0);
}

TEST_F(ApiIntegrationTest, GetMetricsUsesLatestTimestampWhenDateRangeIsOmitted) {
    auto dbClient = drogon::app().getDbClient();
    dbClient->execSqlSync("INSERT INTO sensors (external_id) VALUES ($1)", "sensor-1");
    dbClient->execSqlSync(R"(
        INSERT INTO readings (sensor_id, recorded_at, temperature, humidity, wind_speed)
        SELECT id, $2::timestamptz, $3, $4, $5 FROM sensors WHERE external_id = $1
    )",
                          "sensor-1",
                          "2026-04-01T00:00:00Z",
                          10.0,
                          50.0,
                          nullptr);
    dbClient->execSqlSync(R"(
        INSERT INTO readings (sensor_id, recorded_at, temperature, humidity, wind_speed)
        SELECT id, $2::timestamptz, $3, $4, $5 FROM sensors WHERE external_id = $1
    )",
                          "sensor-1",
                          "2026-04-03T00:00:00Z",
                          40.0,
                          90.0,
                          nullptr);

    const auto config = loadConfig();
    auto client =
        drogon::HttpClient::newHttpClient("http://127.0.0.1:" + std::to_string(config.apiPort));

    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Get);
    request->setPath("/api/v1/metrics?metric=temperature&stat=max&sensorId=sensor-1");

    const auto [result, response] = client->sendRequest(request, 5.0);
    ASSERT_EQ(result, drogon::ReqResult::Ok);
    ASSERT_TRUE(response != nullptr);
    EXPECT_EQ(response->statusCode(), drogon::k200OK);

    const auto json = response->getJsonObject();
    ASSERT_TRUE(json != nullptr);
    EXPECT_EQ((*json)["from"].asString(), "2026-04-03T00:00:00Z");
    EXPECT_EQ((*json)["to"].asString(), "2026-04-03T00:00:00Z");
    ASSERT_EQ((*json)["results"].size(), 1);
    EXPECT_DOUBLE_EQ((*json)["results"][0]["metrics"]["temperature"].asDouble(), 40.0);
}

}  // namespace
/* TODO: 
what happens if no data in range
what happens if no data in sensor
*/
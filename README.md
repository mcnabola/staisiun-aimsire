# Weather Data Service

REST API for ingesting and querying weather sensor data.

## Architecture
Detailed documentation of the system architecture, design rationales, and indexing strategies can be found in [ARCHITECTURE.md](file://ARCHITECTURE.md).

## Stack

- C++20
- Drogon
- CMake
- PostgreSQL

## Key Features

- **High Performance**: Built with C++20 and the Drogon non-blocking framework.
- **Layered Architecture**: Clean separation between Controllers, Services, and Repositories.
- **Automated Migrations**: Self-managing database schema on application startup.
- **Robust Validation**: Centralized validation logic for all sensor inputs.
- **Tested**: Comprehensive unit and database integration test suites.

## Endpoints

- `POST /api/v1/readings`
- `GET /api/v1/metrics`

## Local Setup

### 1. Install dependencies

Install:

- CMake 3.20+
- a C++20 compiler
- Drogon
- PostgreSQL

```bash
brew install postgresql@16
brew reinstall --build-from-source drogon
```

If Drogon was installed before PostgreSQL, the project will now fail at CMake configure time with a clear error instead of failing later at runtime.

### 2. Build

```bash
cmake -S . -B build
cmake --build build
```

If GoogleTest is not already installed on the machine, CMake will fetch it automatically when tests are enabled.

### 3. Run

For local development, make sure PostgreSQL is running:

```bash
brew services start postgresql@16
```

If the API starts before Postgres is ready, the service retries for up to ~20s
before failing.

```bash
./build/weather_data
```

The service reads runtime configuration from `config/config.json`.

### 4. Quick Start (Test the API)

Once the service is running, you can test it using `curl`:

**Ingest a Reading:**
```bash
curl -X POST http://localhost:8080/api/v1/readings \
     -H "Content-Type: application/json" \
     -d '{
       "sensorId": "sensor-1",
       "timestamp": "2026-05-01T12:00:00Z",
       "temperature": 18.5,
       "humidity": 55.0
     }'
```

**Query Aggregated Metrics:**
```bash
curl "http://localhost:8080/api/v1/metrics?sensorId=sensor-1&metric=temperature&stat=avg"
```

## Database

This project does not use versioned migrations. The schema is bootstrapped from
`db/migrations/001_initial_schema.sql` on startup using idempotent
`CREATE ... IF NOT EXISTS` sql queries.

## Testing

### Unit and controller tests

```bash
ctest --test-dir build
or
./build/weather_data_tests
```

These cover:

- validation rules;
- request-model mapping;
- controller behavior for invalid JSON and validation failures.

### Real database integration tests

The project includes integration tests that:

- start the Drogon app on a test port;
- send real HTTP requests to `POST /api/v1/readings`;
- verify persisted rows in PostgreSQL.

```bash
 cmake --build build --target test
 or
 ./build/weather_data_integration_tests
```

The integration tests expect the target PostgreSQL database to already exist.

This project was developed + tested on a macOS environment.

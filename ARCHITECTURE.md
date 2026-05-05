# Weather Sensor Metrics Service - Technical Specification


## 1. Technology Stack
- **Language**: Modern C++ (C++20)
- **Web Framework**: [Drogon](https://drogon.org/) (High-performance, non-blocking I/O)
- **Build System**: CMake 3.20+
- **Database**: PostgreSQL
- **Testing**: GoogleTest, CTest (Unit & Integration tests)

---

## 2. Architecture Overview
The service follows a **Layered Architecture** pattern, ensuring a clean separation of concerns and high testability.

### Layer Responsibilities
- **Controller Layer (`src/controllers`)**:
    - Manages HTTP routing and protocol-specific logic.
    - Deserializes JSON payloads into internal models.
    - Handles HTTP status codes and response formatting.
- **Service Layer (`src/services`)**:
    - Coordinates business logic and cross-component operations.
    - Orchestrates validation via the `ValidationService`.
    - Bridges the gap between web controllers and data persistence.
- **Repository Layer (`src/repositories`)**:
    - Encapsulates all database interactions.
    - Responsible for SQL query construction, execution, and result mapping.
    - Ensures that the rest of the application remains database-agnostic.
- **Model Layer (`src/models`)**:
    - Defines Data Transfer Objects (DTOs) for structured communication between layers.

### Architectural Rationale
- **Separation of Concerns**: By isolating SQL logic in repositories and HTTP logic in controllers, the system becomes significantly easier to maintain and refactor.
- **Testability**: The decoupled design allows for granular unit testing of services and validation logic without requiring a running database or HTTP server.
- **Scalability**: The use of the Drogon framework provides a non-blocking, event-driven architecture capable of handling high concurrency with minimal resource overhead.

---

## 3. Design Rationale

### Choice of PostgreSQL
PostgreSQL was selected over NoSQL alternatives (like DynamoDB) due to the nature of the requirements:
- **Relational Integrity**: Strong consistency and foreign key relationships between sensors and readings.
- **Powerful Aggregations**: Native support for `AVG`, `MIN`, `MAX`, and `SUM` allows the database to perform calculations efficiently near the data. Defer calculation of statistics to the database.
- **Time-Series Capabilities**: Excellent performance for range-based queries on timestamps.

### API Design: GET for Metrics
While complex queries sometimes justify `POST`, a `GET` endpoint was chosen for `/api/v1/metrics`:
- **Idempotency**: Retrieval operations are naturally idempotent and should be cacheable.
- **REST Semantics**: Aligns with standard practices for data retrieval.
- **Simplicity**: The query parameters remain manageable and intuitive.

### Schema Design: Fixed Metric Columns
The schema uses dedicated columns for metrics (`temperature`, `humidity`, `wind_speed`) rather than a generic key-value EAV (Entity-Attribute-Value) model:
- **Performance**: Faster indexing and retrieval compared to dynamic tables.
- **Type Safety**: Metrics are stored as double precision, ensuring data integrity.
- **Simplicity**: Simplifies the C++ model mapping and keeps SQL queries readable.

### Automated Database Schema Bootstrap
The service bootstraps the schema from `db/migrations/001_initial_schema.sql` at startup using idempotent sql queries:
- **Consistency**: Guarantees that every instance of the service (Dev, CI, Prod) runs on an identical schema.
- **Onboarding**: Removes the need for manual SQL schema setup for new developers.

---

## 4. API Specification

### Ingest Readings
`POST /api/v1/readings`

Ingests a single reading for a sensor. If the sensor does not exist, it is created automatically.

**Request Body:**
```json
{
  "sensorId": "sensor-1",
  "timestamp": "2026-04-24T10:15:00Z",
  "temperature": 18.7,
  "humidity": 56.2,
  "windSpeed": 8.4
}
```

**Response (201 Created):**
```json
{
  "status": "accepted",
  "readingId": "550e8400-e29b-41d4-a716-446655440000"
}
```

**Validation Rules:**
- `sensorId`: Required, non-empty string.
- `timestamp`: Required, valid ISO-8601 UTC format.
- At least one metric field must be present.
- Metric fields must be numeric.

**Behavior Notes:**
A repeated identical request is treated as a new reading, not idempotent.
We do not know what metric of measurement is being taken, celsius or fahrenheit for example.

### Query Aggregated Metrics
`GET /api/v1/metrics`

Retrieves aggregated statistics for one or more sensors.

**Query Parameters:**
- `sensorId`: (Optional) Comma-separated list of sensor IDs. If omitted, all sensors are queried.
- `metric`: (Required) Comma-separated list of metrics (`temperature`, `humidity`, `windSpeed`).
- `stat`: (Required) One of `min`, `max`, `sum`, `avg`.
- `from`: (Optional) Start ISO-8601 timestamp.
- `to`: (Optional) End ISO-8601 timestamp.

**Response (200 OK):**
```json
{
  "statistic": "avg",
  "from": "2026-04-17T00:00:00Z",
  "to": "2026-04-24T00:00:00Z",
  "results": [
    {
      "sensorId": "sensor-1",
      "metrics": {
        "temperature": 17.93,
        "humidity": 58.11
      }
    }
  ]
}
```

**Behavior Notes:**
- If `from`/`to` are omitted, the API defaults to the latest 24-hour window based on the most recent data available.
- Sensors with no data in the requested range are omitted from the response.

**Design Notes:**

Using comma separated formatting for sensorId, metric, allowed the code parsing to be much more simple and maintainable.
I also thought this API design was less verbose this way (less query parameters).
API easier to read, easier to write in curl, easier to parse. All wins in my book.

The design document mentions "average", for the sake of simplification the code only accepts "avg" as a valid statistic name.
The design doc also lists "min" and "max" in their shortened forms. This was justification for doing so with "avg".

iso8601 timestamp parsing: the version of the standard library on this system (Apple Clang 17) does not yet include std::chrono::parse. Regex is sufficient for our current needs.
Gaps: It rejects timestamps with milliseconds (e.g., 2026-04-24T10:15:00.000Z).
It rejects UTC offsets other than Z (e.g., +00:00
Fix: use std::chrono::parse when available

---

## 5. Error Handling

Errors use a consistent JSON structure.

Example:

```json
{
  "error": {
    "code": "VALIDATION_ERROR",
    "message": "At least one metric parameter is required"
  }
}
```

Status codes used:

- `200 OK` for successful queries
- `201 Created` or `202 Accepted` for successful ingestion
- `400 Bad Request` for malformed JSON or invalid parameters
- `422 Unprocessable Entity` optionally for semantic validation failures
- `500 Internal Server Error` for unexpected failures

---

## 6. Data Model

### Tables
#### `sensors`
- `id`: UUID (Primary Key)
- `external_id`: TEXT (Unique, Index)
- `created_at`: TIMESTAMPTZ

#### `readings`
- `id`: UUID (Primary Key)
- `sensor_id`: UUID (Foreign Key -> sensors.id)
- `recorded_at`: TIMESTAMPTZ (Index)
- `temperature`: DOUBLE PRECISION
- `humidity`: DOUBLE PRECISION
- `wind_speed`: DOUBLE PRECISION

### Indexing Strategy
- **`sensors.external_id`**: Unique index for high speed lookups during ingestion.
- **`readings.recorded_at`**: Index for efficient range-based filtering.
- **`readings(sensor_id, recorded_at)`**: Compound index to optimize per-sensor aggregations over time.

---

## 7. Project Structure
```text
.
├── CMakeLists.txt
├── config/             # Configuration files
├── db/                 # Database schema (bootstrap)
├── src/
│   ├── controllers/    # HTTP Request handlers
│   ├── models/         # DTOs and internal models
│   ├── repositories/   # Data access logic
│   ├── services/       # Business logic and validation
│   └── main.cpp        # Application entry point
├── tests/              # Unit and Integration tests
└── ARCHITECTURE.md     # This document
```

---

## 8. Implementation Notes
- **API Versioning**: Prefixed with `/api/v1/` to allow for future breaking changes without impacting existing clients.
- **Case Sensitivity**: The API uses `camelCase` for JSON keys (standard for REST) and maps them to `snake_case` in the database (standard for PostgreSQL).
- **Security**: SQL queries use prepared statements with the Drogon binder to prevent SQL injection.
- **Efficiency**: Statistic calculation is deferred to the database engine, ensuring minimal data transfer and optimal performance.

---

## 9. Future Evolution
- **Scalability**:
  - pagination
  - caching
- **Deployment**:
  - containerized deployment.
- **Security**:
  - JWT-based authentication
  - rate limiting.
- **Flexibility**: Transition to a more dynamic metric storage model if requirements expand beyond weather data.
- **Observability**: Integrate structured logging and OpenTelemetry for tracing.

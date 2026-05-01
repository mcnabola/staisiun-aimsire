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

### 3.1 Layer Responsibilities
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

### 3.2 Architectural Rationale
- **Separation of Concerns**: By isolating SQL logic in repositories and HTTP logic in controllers, the system becomes significantly easier to maintain and refactor.
- **Testability**: The decoupled design allows for granular unit testing of services and validation logic without requiring a running database or HTTP server.
- **Scalability**: The use of the Drogon framework provides a non-blocking, event-driven architecture capable of handling high concurrency with minimal resource overhead.

---

## 3. Design Rationale

### 4.1 Choice of PostgreSQL
PostgreSQL was selected over NoSQL alternatives (like DynamoDB) due to the nature of the requirements:
- **Relational Integrity**: Strong consistency and foreign key relationships between sensors and readings.
- **Powerful Aggregations**: Native support for `AVG`, `MIN`, `MAX`, and `SUM` allows the database to perform calculations efficiently near the data. Defer calculation of statistics to the database.
- **Time-Series Capabilities**: Excellent performance for range-based queries on timestamps.

### 4.2 API Design: GET for Metrics
While complex queries sometimes justify `POST`, a `GET` endpoint was chosen for `/api/v1/metrics`:
- **Idempotency**: Retrieval operations are naturally idempotent and should be cacheable.
- **REST Semantics**: Aligns with standard practices for data retrieval.
- **Simplicity**: The query parameters remain manageable and intuitive.

### 4.3 Schema Design: Fixed Metric Columns
The schema uses dedicated columns for metrics (`temperature`, `humidity`, `wind_speed`) rather than a generic key-value EAV (Entity-Attribute-Value) model:
- **Performance**: Faster indexing and retrieval compared to dynamic tables.
- **Type Safety**: Metrics are stored as double precision, ensuring data integrity.
- **Simplicity**: Simplifies the C++ model mapping and keeps SQL queries readable.

### 4.4 Automated Database Migrations
A custom-built `DatabaseMigrationService` is integrated into the application startup:
- **Consistency**: Guarantees that every instance of the service (Dev, CI, Prod) runs on an identical schema.
- **Onboarding**: Removes the need for manual SQL setup for new developers.

---

## 4. API Specification

### 5.1 Ingest Readings
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

### 5.2 Query Aggregated Metrics
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

---

## 5. Data Model

### 6.1 Tables
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

### 6.2 Indexing Strategy
- **`sensors.external_id`**: Unique index for high speed lookups during ingestion.
- **`readings.recorded_at`**: Index for efficient range-based filtering.
- **`readings(sensor_id, recorded_at)`**: Compound index to optimize per-sensor aggregations over time.

---

## 6. Project Structure
```text
.
├── CMakeLists.txt
├── config/             # Configuration files
├── db/                 # Database migrations
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

## 7. Implementation Notes (Reviewer-Facing)
- **API Versioning**: Prefixed with `/api/v1/` to allow for future breaking changes without impacting existing clients.
- **Case Sensitivity**: The API uses `camelCase` for JSON keys (standard for REST) and maps them to `snake_case` in the database (standard for PostgreSQL).
- **Security**: SQL queries use prepared statements with the Drogon binder to prevent SQL injection.
- **Efficiency**: Statistic calculation is deferred to the database engine, ensuring minimal data transfer and optimal performance.

---

## 8. Future Evolution
- **Scalability**: Move to Docker Compose for containerized deployment of API and Postgres.
- **Security**: Implement JWT-based authentication and rate limiting.
- **Flexibility**: Transition to a more dynamic metric storage model if requirements expand beyond weather data.
- **Observability**: Integrate structured logging and OpenTelemetry for tracing.
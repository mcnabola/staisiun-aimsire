cmake_minimum_required(VERSION 3.20)
project(weather_data VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build tests" ON)

find_package(PostgreSQL REQUIRED)
find_package(Drogon CONFIG REQUIRED)

function(verify_drogon_postgresql_support)
    if(NOT TARGET Drogon::Drogon)
        message(FATAL_ERROR "Drogon::Drogon target not found after find_package(Drogon).")
    endif()

    get_target_property(DROGON_LIBRARY Drogon::Drogon IMPORTED_LOCATION)
    if(NOT DROGON_LIBRARY)
        get_target_property(DROGON_LIBRARY Drogon::Drogon IMPORTED_LOCATION_RELEASE)
    endif()
    if(NOT DROGON_LIBRARY)
        get_target_property(DROGON_LIBRARY Drogon::Drogon LOCATION)
    endif()

    if(NOT DROGON_LIBRARY OR NOT EXISTS "${DROGON_LIBRARY}")
        message(WARNING
            "Could not determine the Drogon library path during configure. "
            "Skipping PostgreSQL capability verification.")
        return()
    endif()

    if(APPLE)
        execute_process(
            COMMAND otool -L "${DROGON_LIBRARY}"
            OUTPUT_VARIABLE DROGON_LINK_INFO
            ERROR_QUIET
        )
    elseif(UNIX)
        execute_process(
            COMMAND ldd "${DROGON_LIBRARY}"
            OUTPUT_VARIABLE DROGON_LINK_INFO
            ERROR_QUIET
        )
    else()
        message(WARNING
            "Automatic Drogon PostgreSQL verification is not implemented for this platform.")
        return()
    endif()

    string(FIND "${DROGON_LINK_INFO}" "libpq" DROGON_HAS_LIBPQ)
    if(DROGON_HAS_LIBPQ EQUAL -1)
        message(FATAL_ERROR
            "Drogon was found, but it does not appear to be built with PostgreSQL support.\n"
            "Install PostgreSQL first, then rebuild/reinstall Drogon.\n"
            "On macOS with Homebrew:\n"
            "  brew install postgresql@16\n"
            "  brew reinstall --build-from-source drogon")
    endif()
endfunction()

verify_drogon_postgresql_support()

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/controllers/ReadingController.cpp
    src/controllers/MetricsController.cpp
    src/services/ValidationService.cpp
    src/services/ReadingService.cpp
    src/services/MetricsService.cpp
    src/repositories/ReadingRepository.cpp
    src/services/DatabaseSchemaService.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Drogon::Drogon
        PostgreSQL::PostgreSQL
)

if(BUILD_TESTING)
    enable_testing()
    include(FetchContent)
    find_package(GTest CONFIG QUIET)

    if(NOT GTest_FOUND)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    add_executable(${PROJECT_NAME}_tests
        tests/ValidationServiceTest.cpp
        tests/ReadingControllerTest.cpp
        tests/MetricsControllerTest.cpp
        src/services/ValidationService.cpp
        src/controllers/ReadingController.cpp
        src/services/ReadingService.cpp
        src/controllers/MetricsController.cpp
        src/services/MetricsService.cpp
        src/repositories/ReadingRepository.cpp
        src/services/DatabaseSchemaService.cpp
    )

    target_include_directories(${PROJECT_NAME}_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(${PROJECT_NAME}_tests
        PRIVATE
            Drogon::Drogon
            PostgreSQL::PostgreSQL
            GTest::gtest_main
    )

    add_executable(${PROJECT_NAME}_integration_tests
        tests/ApiIntegrationTest.cpp
        src/controllers/ReadingController.cpp
        src/controllers/MetricsController.cpp
        src/services/ValidationService.cpp
        src/services/ReadingService.cpp
        src/services/MetricsService.cpp
        src/repositories/ReadingRepository.cpp
        src/services/DatabaseSchemaService.cpp
    )

    target_include_directories(${PROJECT_NAME}_integration_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_compile_definitions(${PROJECT_NAME}_integration_tests
        PRIVATE
            WEATHER_DATA_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(${PROJECT_NAME}_integration_tests
        PRIVATE
            Drogon::Drogon
            PostgreSQL::PostgreSQL
            GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(${PROJECT_NAME}_tests)
    gtest_discover_tests(${PROJECT_NAME}_integration_tests)
endif()

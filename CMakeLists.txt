cmake_minimum_required(VERSION 3.20)
project(weather_data VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build tests" ON)

find_package(Drogon CONFIG REQUIRED)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/controllers/HealthController.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Drogon::Drogon
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
        src/services/ValidationService.cpp
    )

    target_include_directories(${PROJECT_NAME}_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(${PROJECT_NAME}_tests
        PRIVATE
            GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(${PROJECT_NAME}_tests)
endif()

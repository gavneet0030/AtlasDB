cmake_minimum_required(VERSION 3.25)

project(AtlasDB
    VERSION 0.1.0
    DESCRIPTION "High-performance analytical database and query execution engine"
    LANGUAGES CXX
)

option(ATLASDB_BUILD_TESTS "Build AtlasDB tests" ON)
option(ATLASDB_ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(ATLASDB_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan in Debug builds" OFF)
option(ATLASDB_ENABLE_PARALLEL "Enable parallel execution components" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(Threads REQUIRED)

add_library(atlasdb_options INTERFACE)
target_include_directories(atlasdb_options INTERFACE
    "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries(atlasdb_options INTERFACE Threads::Threads)

if(ATLASDB_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(atlasdb_options INTERFACE /W4 /permissive-)
    else()
        target_compile_options(atlasdb_options INTERFACE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow
        )
    endif()
endif()

if(ATLASDB_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(atlasdb_options INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    )
    target_link_options(atlasdb_options INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
endif()

add_library(atlasdb_core
    src/common/version.cpp
    src/types/data_type.cpp
    src/types/value.cpp
    src/catalog/schema.cpp
    src/catalog/database_catalog.cpp
    src/storage/column_vector.cpp
    src/storage/record_batch.cpp
    src/storage/table.cpp
    src/storage/table_storage.cpp
    src/execution/operators/table_scan.cpp
    src/execution/operators/filter.cpp
    src/execution/operators/projection.cpp
    src/execution/operators/aggregation.cpp
    src/execution/operators/sort.cpp
    src/execution/expressions/literal_expression.cpp
    src/execution/expressions/column_reference_expression.cpp
    src/execution/expressions/comparison_expression.cpp
        src/execution/expressions/logical_expression.cpp
    src/parser/lexer.cpp
    src/parser/parser.cpp
    src/execution/query_planner.cpp
    src/execution/sql_executor.cpp
    src/execution/database_sql_executor.cpp
)
target_link_libraries(atlasdb_core PUBLIC atlasdb_options)

add_executable(atlasdb src/main.cpp)
target_link_libraries(atlasdb PRIVATE atlasdb_core)

if(ATLASDB_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.16.0
        GIT_SHALLOW TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(atlasdb_tests
        tests/unit/test_smoke.cpp
        tests/unit/test_value.cpp
        tests/unit/test_schema.cpp
        tests/unit/test_database_catalog.cpp
    tests/unit/test_column_vector.cpp
    tests/unit/test_record_batch.cpp
    tests/unit/test_table.cpp
        tests/unit/test_table_scan.cpp
        tests/unit/test_filter.cpp
        tests/unit/test_execution_pipeline.cpp
        tests/unit/test_expressions.cpp
        tests/unit/test_projection.cpp
        tests/unit/test_aggregation.cpp
        tests/unit/test_sort.cpp
        tests/unit/test_lexer.cpp
        tests/unit/test_parser.cpp
        tests/unit/test_query_planner.cpp
        tests/unit/test_sql_executor.cpp
        tests/unit/test_database_sql_executor.cpp
        tests/unit/test_table_storage.cpp
    )
    target_link_libraries(atlasdb_tests PRIVATE
        atlasdb_core
        GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(atlasdb_tests)

    if(WIN32)
        find_program(ATLASDB_POWERSHELL_EXECUTABLE
            NAMES pwsh.exe pwsh powershell.exe powershell
        )
        if(ATLASDB_POWERSHELL_EXECUTABLE)
            add_test(
                NAME AtlasDBCliIntegration
                COMMAND "${ATLASDB_POWERSHELL_EXECUTABLE}"
                    -NoProfile
                    -ExecutionPolicy Bypass
                    -File "${PROJECT_SOURCE_DIR}/tests/integration/cli_integration.ps1"
                    "$<TARGET_FILE:atlasdb>"
            )
            set_tests_properties(AtlasDBCliIntegration PROPERTIES TIMEOUT 90)
        else()
            message(WARNING "PowerShell not found; CLI integration test was not registered.")
        endif()
    elseif(UNIX)
        find_program(ATLASDB_BASH_EXECUTABLE NAMES bash)
        if(ATLASDB_BASH_EXECUTABLE)
            add_test(
                NAME AtlasDBCliIntegration
                COMMAND "${ATLASDB_BASH_EXECUTABLE}"
                    "${PROJECT_SOURCE_DIR}/tests/integration/cli_integration.sh"
                    "$<TARGET_FILE:atlasdb>"
            )
            set_tests_properties(AtlasDBCliIntegration PROPERTIES TIMEOUT 90)
        else()
            message(WARNING "Bash not found; CLI integration test was not registered.")
        endif()
    endif()
endif()

install(TARGETS atlasdb RUNTIME DESTINATION bin)

message(STATUS "AtlasDB version: ${PROJECT_VERSION}")
message(STATUS "C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Tests enabled: ${ATLASDB_BUILD_TESTS}")

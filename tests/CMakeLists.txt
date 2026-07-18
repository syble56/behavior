# Unit Tests for Behavior SDK — storage / aggregator / queue / migrator
#
# 依赖：Qt5/6 (Sql, Core) + GoogleTest + behavior_sdk 静态库
# 框架：GoogleTest
#
# 用法：
#   cmake -B build/test -S . -DBEHAVIOR_BUILD_TESTS=ON
#   cmake --build build/test
#   ctest --test-dir build/test --output-on-failure

# --- Qt Test 模块 ---
if(QT_VER EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS Test Sql Core)
else()
    find_package(Qt5 REQUIRED COMPONENTS Test Sql Core)
endif()

# --- GoogleTest via FetchContent ---
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

# 测试可执行文件列表
set(BEHAVIOR_TESTS
    TestDatabase
    TestAggregator
    TestOperationQueue
    TestBatchWriter
    TestMigrator
    TestQueryFilter
    TestDistanceCalc
    TestAnalyzerOutput
    TestCoordBoundary
    TestTimezone
    TestAggPrecision
    TestAnalyzers
    TestCore
    TestEventProcessor
    TestCalendarPicker
)

foreach(TEST_NAME ${BEHAVIOR_TESTS})
    add_executable(${TEST_NAME} ${TEST_NAME}.cpp)
    set_target_properties(${TEST_NAME} PROPERTIES AUTOMOC ON)
    target_link_libraries(${TEST_NAME}
        PRIVATE
            behavior_sdk
            GTest::gtest
            ${QT_PREFIX}::Test
            ${QT_PREFIX}::Sql
            ${QT_PREFIX}::Core
            ${QT_PREFIX}::Widgets
    )
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/demo ${CMAKE_SOURCE_DIR}/demo/date_picker)
    target_sources(${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp
        ${CMAKE_SOURCE_DIR}/demo/date_picker/calendar_picker.cpp
        ${CMAKE_SOURCE_DIR}/demo/date_picker/dropdown_popup.cpp
        ${CMAKE_SOURCE_DIR}/demo/date_picker/date_field.cpp
    )
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

# --- 数据填充工具 ---
add_executable(seed_7days seed_7days.cpp)
set_target_properties(seed_7days PROPERTIES AUTOMOC ON)
target_link_libraries(seed_7days
    PRIVATE
        behavior_sdk
        ${QT_PREFIX}::Sql
        ${QT_PREFIX}::Core
)

# --- 聚合一致性检查 ---
add_executable(check_agg check_agg.cpp)
set_target_properties(check_agg PROPERTIES AUTOMOC ON)
target_link_libraries(check_agg
    PRIVATE
        behavior_sdk
        ${QT_PREFIX}::Sql
        ${QT_PREFIX}::Core
)

add_executable(check_op_stats check_op_stats.cpp)
set_target_properties(check_op_stats PROPERTIES AUTOMOC ON)
target_link_libraries(check_op_stats
    PRIVATE
        behavior_sdk
        ${QT_PREFIX}::Sql
        ${QT_PREFIX}::Core
)

add_executable(query_0701 query_0701.cpp)
set_target_properties(query_0701 PROPERTIES AUTOMOC ON)
target_link_libraries(query_0701
    PRIVATE
        behavior_sdk
        ${QT_PREFIX}::Sql
        ${QT_PREFIX}::Core
)

add_executable(query_op_detail query_op_detail.cpp)
set_target_properties(query_op_detail PROPERTIES AUTOMOC ON)
target_link_libraries(query_op_detail
    PRIVATE
        behavior_sdk
        ${QT_PREFIX}::Sql
        ${QT_PREFIX}::Core
)

add_executable(check_dialog check_dialog.cpp)
set_target_properties(check_dialog PROPERTIES AUTOMOC ON)
target_link_libraries(check_dialog
    PRIVATE
        behavior_sdk
        ${QT_PREFIX}::Sql
        ${QT_PREFIX}::Core
)

add_executable(query_dialog_today query_dialog_today.cpp)
set_target_properties(query_dialog_today PROPERTIES AUTOMOC ON)
target_link_libraries(query_dialog_today
    PRIVATE
        behavior_sdk
        ${QT_PREFIX}::Sql
        ${QT_PREFIX}::Core
)

# --- 压力测试 ---
add_executable(stress_test stress_test.cpp)
set_target_properties(stress_test PROPERTIES AUTOMOC ON)
target_link_libraries(stress_test
    PRIVATE
        behavior_sdk
        ${QT_PREFIX}::Sql
        ${QT_PREFIX}::Core
)

# --- 异步清理器测试 ---
add_executable(test_cleaner test_cleaner.cpp)
set_target_properties(test_cleaner PROPERTIES AUTOMOC ON)
target_link_libraries(test_cleaner
    PRIVATE
        behavior_sdk
        ${QT_PREFIX}::Sql
        ${QT_PREFIX}::Core
)

# --- 数据填充工具 ---
add_executable(fill_data fill_data.cpp)
set_target_properties(fill_data PROPERTIES AUTOMOC ON)
target_link_libraries(fill_data
    PRIVATE
        behavior_sdk
        ${QT_PREFIX}::Sql
        ${QT_PREFIX}::Core
)

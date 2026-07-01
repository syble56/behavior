# Unit Tests for Behavior SDK — storage / aggregator / queue / migrator
#
# 依赖：Qt5/6 (Test, Sql, Core) + behavior_sdk 静态库
# 框架：QtTestLib（无外部依赖，与项目技术栈一致）
#
# 用法：
#   cmake -B build/test -S . -DBEHAVIOR_BUILD_TESTS=ON
#   cmake --build build/test
#   ctest --test-dir build/test --output-on-failure

# --- Qt Test 模块 ---
if(QT_VER EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS Test)
else()
    find_package(Qt5 REQUIRED COMPONENTS Test)
endif()

# --- GoogleTest 风格的 main 不需要，QtTest 自带 QTEST_MAIN ---

# 测试可执行文件列表
set(BEHAVIOR_TESTS
    TestDatabase
    TestAggregator
    TestOperationQueue
    TestBatchWriter
    TestMigrator
    TestQueryFilter
)

foreach(TEST_NAME ${BEHAVIOR_TESTS})
    add_executable(${TEST_NAME} ${TEST_NAME}.cpp)
    set_target_properties(${TEST_NAME} PROPERTIES AUTOMOC ON)
    target_link_libraries(${TEST_NAME}
        PRIVATE
            behavior_sdk
            ${QT_PREFIX}::Test
            ${QT_PREFIX}::Sql
            ${QT_PREFIX}::Core
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

# Behavior SDK — 单元测试 (UT)

## 概述

针对数据库存入、聚合、查询等核心逻辑设计的单元测试套件，使用 **QtTestLib** 框架（与项目技术栈一致，无额外依赖）。

## 测试矩阵

| 测试文件 | 覆盖模块 | 用例数 | 关键验证点 |
|---------|---------|-------|-----------|
| `TestDatabase.cpp` | Database (CRUD/Schema/线程) | 20 | 建表建索引、单条/批量插入、时间范围/会话/事件类型/主窗口过滤、limit、count、会话插入更新、过期清理、数据往返完整性、optional duration、负热力区、多线程连接 |
| `TestAggregator.cpp` | Aggregator (6种聚合) | 16 | 操作聚合(hour/day)、空范围、action_key回退、模块聚合+unknown、输入方式全覆盖、热力图仅主窗口+仅点击事件、对话框duration计算、时间分布跨小时桶、幂等性(INSERT OR REPLACE)、信号发射、粒度格式 |
| `TestOperationQueue.cpp` | OperationQueue (队列/背压) | 12 | 入队出队、FIFO顺序、move语义、高水位丢弃+计数、waitForData超时/可用、多生产者多消费者并发、边界值(0/负数) |
| `TestBatchWriter.cpp` | BatchWriter (线程化写入) | 6 | 启动停止、flush全写、written信号、空队列不写、batchSize限制、stop自动flush |
| `TestMigrator.cpp` | Migrator (版本管理) | 5 | 全新库版本、get/set版本、幂等迁移、v1→v2迁移 |
| `TestQueryFilter.cpp` | QueryFilter (组合查询) | 6 | 多条件组合、边界精确匹配、无结果、limit=0、远未来时间戳、特殊字符session_id |

**总计：65 个测试用例**

## 测试分类说明

### 1. 数据库 CRUD (`TestDatabase`)

| 类别 | 用例 | 验证目标 |
|------|------|---------|
| Schema | `testOpenCreatesAllTables` | 9张表全部创建 |
| Schema | `testOpenCreatesIndexes` | 关键索引存在 |
| Schema | `testReopenSamePath` | 关闭重开不丢数据 |
| 插入 | `testInsertSingleOperation` | 单条插入+查回 |
| 插入 | `testBatchInsert` | 50条批量插入 |
| 插入 | `testBatchInsertEmpty` | 空批量不报错 |
| 插入 | `testInsertAllEventTypes` | 6种EventType全部正确存取 |
| 查询 | `testQueryByTimeRange` | 时间范围精确+结果有序 |
| 查询 | `testQueryBySessionId` | 按会话过滤 |
| 查询 | `testQueryByEventType` | 按事件类型过滤 |
| 查询 | `testQueryOnlyMainWindow` | 仅主窗口过滤 |
| 查询 | `testQueryLimit` | LIMIT截断 |
| 查询 | `testCountOperations` / `testCountEmptyRange` | 计数正确 |
| 会话 | `testInsertSession` / `testUpdateSession` | 会话插入+更新 |
| 清理 | `testCleanOldData` | 过期数据删除，当前数据保留 |
| 完整性 | `testOperationRoundTrip` | 全字段写入→读出无损 |
| 完整性 | `testDurationOptional` | optional<int> 的 null 和有值 |
| 完整性 | `testHeatRegionNegative` | -1 值正确存储 |
| 线程 | `testConnectionPerThread` | 不同线程获取独立连接 |

### 2. 聚合逻辑 (`TestAggregator`)

| 类别 | 用例 | 验证目标 |
|------|------|---------|
| 操作聚合 | `testAggregateOperationsByHour` | 按小时聚合count |
| 操作聚合 | `testAggregateOperationsByDay` | 按天聚合count |
| 操作聚合 | `testAggregateOperationsEmptyRange` | 空范围不崩溃 |
| 操作聚合 | `testAggregateOperationsActionKeyFallback` | COALESCE回退链：action_name→control_name→control_class→event_type |
| 模块聚合 | `testAggregateModules` | 按window_class统计 |
| 模块聚合 | `testAggregateModulesUnknownClass` | 空window_class→'unknown' |
| 输入聚合 | `testAggregateInputs` | mouse/keyboard计数 |
| 输入聚合 | `testAggregateInputsAllMethods` | 三种输入方式都有 |
| 热力图 | `testAggregateHeatmapOnlyMainWindow` | 仅is_main_window=1 |
| 热力图 | `testAggregateHeatmapExcludesNonClickEvents` | 仅click/tap/area事件 |
| 对话框 | `testAggregateDialogs` | open_count/total_duration/avg_duration |
| 对话框 | `testAggregateDialogsDurationCalc` | duration为null时COALESCE为0 |
| 时间分布 | `testTimeDistribution` | 正确归入对应小时 |
| 时间分布 | `testTimeDistributionHourlyBuckets` | 跨3小时分桶正确 |
| 幂等性 | `testAggregateIdempotent` | 重复聚合不翻倍（INSERT OR REPLACE） |
| 信号 | `testAggregationCompletedSignal` | aggregationCompleted信号发射 |
| 粒度 | `testGranularityFormat` | Hour→"yyyy-MM-dd HH:00"，Day→"yyyy-MM-dd" |

### 3. 操作队列 (`TestOperationQueue`)

| 用例 | 验证目标 |
|------|---------|
| `testEnqueueDequeue` | 基本入队出队 |
| `testDequeueEmpty` | 空队列出队 |
| `testDequeuePartial` | 部分出队，剩余正确 |
| `testSize` | size()实时准确 |
| `testFIFO` | 100条严格FIFO |
| `testMoveSemantics` | move入队不拷贝 |
| `testHighWatermarkDrop` | 50000高水位后丢弃 |
| `testDroppedCount` | 丢弃计数准确 |
| `testWaitForDataTimeout` | 超时返回false |
| `testWaitForDataAvailable` | 有数据返回true |
| `testConcurrentProducerConsumer` | 4生产者×1000条，消费者全收 |
| `testDequeueZero` / `testDequeueNegative` | 边界值不崩溃 |

### 4. 批量写入 (`TestBatchWriter`)

| 用例 | 验证目标 |
|------|---------|
| `testStartStop` | 线程正确启停 |
| `testFlushWritesAll` | flush写入全部25条 |
| `testWrittenSignal` | written信号携带正确count |
| `testEmptyQueueNoWrite` | 空队列不触发写入 |
| `testBatchSizeRespected` | 单批次不超batchSize |
| `testStopFlushesRemaining` | stop时自动flush残留数据 |

### 5. 迁移 (`TestMigrator`)

| 用例 | 验证目标 |
|------|---------|
| `testFreshDbGetsCurrentVersion` | 新库版本=CURRENT_VERSION |
| `testGetVersionAfterOpen` | 版本可读 |
| `testSetVersion` | 版本可写 |
| `testMigrateIdempotent` | 重复migrate不变 |
| `testMigrateFromV1` | v1→v2迁移成功 |

### 6. 查询过滤器 (`TestQueryFilter`)

| 用例 | 验证目标 |
|------|---------|
| `testCombinedFilters` | session+event+mainWindow组合 |
| `testExactBoundary` | time >= start AND time <= end 边界 |
| `testNoResults` | 范围外无结果 |
| `testLimitZero` | limit=0返回空 |
| `testLargeTimestamp` | 2099年远未来时间戳 |
| `testSpecialCharsInSessionId` | SQL注入防护+特殊字符 |

## 编译与运行

```bash
# 配置（启用测试）
cmake -B build/test -S . \
  -DCMAKE_PREFIX_PATH="D:/Qt/5.15.2/mingw81_64" \
  -DBEHAVIOR_BUILD_TESTS=ON \
  -G "MinGW Makefiles"

# 编译
cmake --build build/test

# 运行全部测试
ctest --test-dir build/test --output-on-failure

# 运行单个测试
.\build\test\bin\TestDatabase.exe
.\build\test\bin\TestAggregator.exe
.\build\test\bin\TestOperationQueue.exe
```

## 设计原则

1. **隔离性** — 每个用例用 `QTemporaryDir` 创建独立临时数据库，互不干扰
2. **确定性** — 使用固定时间点（如 2026-06-15 10:30:00），不依赖当前时间
3. **边界覆盖** — 空输入、零值、负值、最大值、特殊字符
4. **并发安全** — 多线程生产者/消费者、跨线程数据库连接
5. **幂等性** — 聚合重复执行结果不变（INSERT OR REPLACE）
6. **数据完整性** — 全字段 round-trip 验证，optional 字段 null/有值
7. **SQL注入** — 特殊字符 session_id 测试

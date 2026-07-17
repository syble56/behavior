# 用户行为分析基座 - 需求规格说明书

**文档编号：** SRS-BEHAVIOR-002  
**版本：** V2.1  
**日期：** 2026-07-17  
**密级：** 内部公开

---

## 文档修订记录

| 日期 | 版本 | 修订内容 | 作者 |
|------|------|---------|------|
| 2026-06-17 | V2.0 | 推倒重来，重新设计 | admin |
| 2026-07-17 | V2.1 | 对照实际实现修正：补充聚合表、module字段、input_method枚举、API、性能指标 | admin |

---

## 目录

1. 引言
2. 总体描述
3. 系统特性（功能需求）
4. 数据需求
5. 外部接口需求
6. 质量属性需求
7. 约束
8. 需求追溯矩阵

---

## 1. 引言

### 1.1 编写目的

本文档定义用户行为分析基座的软件需求规格，为开发、测试和维护提供依据。

**预期读者：** 开发人员、测试人员、项目管理人员

### 1.2 项目背景

为仪器仪表软件开发用户行为数据采集和分析功能，用于：
- 了解用户操作习惯
- 优化用户体验
- 发现界面问题
- 支持项目自定义分析

### 1.3 项目目标

| 目标 | 描述 | 优先级 |
|------|------|--------|
| 通用基座 | 提供开箱即用的采集、存储、分析能力 | 高 |
| 可扩展 | 项目可自定义分析器和图表 | 高 |
| 高性能 | 不影响UI流畅度 | 高 |
| 易集成 | 集成到ui_shared，一行初始化 | 高 |

### 1.4 范围

**包含：**
- 数据采集功能
- 数据存储功能
- 通用分析功能
- 图表可视化功能
- 会话管理功能

**不包含：**
- 远程数据传输
- 用户分群分析
- 业务流程分析

---

## 2. 总体描述

### 2.1 产品视角

```
┌─────────────────────────────────────────┐
│           应用层（各项目）                │
│  项目A              项目B                │
│  ├─业务代码         ├─业务代码            │
│  └─自定义分析       └─自定义分析          │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│           ui_shared 共享库               │
│  ┌─────────────────────────────────┐    │
│  │     behavior 模块（本系统）      │    │
│  │  ┌─────────────────────────┐    │    │
│  │  │ 采集层                  │    │    │
│  │  └─────────────────────────┘    │    │
│  │  ┌─────────────────────────┐    │    │
│  │  │ 存储层                  │    │    │
│  │  └─────────────────────────┘    │    │
│  │  ┌─────────────────────────┐    │    │
│  │  │ 分析层                  │    │    │
│  │  └─────────────────────────┘    │    │
│  │  ┌─────────────────────────┐    │    │
│  │  │ 可视化层                │    │    │
│  │  └─────────────────────────┘    │    │
│  └─────────────────────────────────┘    │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│           基础设施层                      │
│  Qt框架    SQLite    Qwt图表库            │
└─────────────────────────────────────────┘
```

### 2.2 用户类

| 用户类 | 描述 |
|--------|------|
| 项目开发者 | 集成基座到项目，可能开发自定义分析器 |
| 最终用户 | 使用仪器仪表软件的操作人员 |
| 产品经理 | 查看分析报告，优化产品设计 |

### 2.3 运行环境

| 环境 | 要求 |
|------|------|
| 操作系统 | Windows / Linux / ARM |
| UI框架 | Qt 5.x 或 Qt 6.x |
| 数据库 | SQLite 3.x |
| 图表库 | Qwt 6.x |
| 编译器 | C++17及以上 |
| 构建工具 | CMake 3.15+ |

---

## 3. 系统特性（功能需求）

### 3.1 数据采集

#### 3.1.1 描述

自动采集用户操作行为，包括鼠标点击、触屏点击、快捷键、对话框操作。采集过程不影响UI流畅度。

#### 3.1.2 功能需求

**[BEHAVIOR.COLLECT.INIT.1]** 系统应支持通过一行代码初始化采集功能。
- Rationale: 降低集成成本
- Priority: 高

**[BEHAVIOR.COLLECT.EVENT.1]** 系统应自动采集鼠标点击事件（mouse_click）。
- Rationale: 最主要的交互方式
- Priority: 高

**[BEHAVIOR.COLLECT.EVENT.2]** 系统应自动采集触屏点击事件（touch_tap）。
- Rationale: 支持触屏设备
- Priority: 高

**[BEHAVIOR.COLLECT.EVENT.3]** 系统应自动采集快捷键操作（shortcut），并从QAction自动识别功能名称。
- Rationale: 分析用户操作习惯
- Priority: 高

**[BEHAVIOR.COLLECT.EVENT.4]** 系统应自动检测对话框打开事件（dialog_open）。
- Rationale: 支持对话框统计分析
- Priority: 高

**[BEHAVIOR.COLLECT.EVENT.5]** 系统应自动检测对话框关闭事件（dialog_close），并计算保留时长。
- Rationale: 支持对话框统计分析
- Priority: 高

**[BEHAVIOR.COLLECT.FILTER.1]** 系统应只采集最内层控件的事件，忽略冒泡到父窗口的事件。
- Rationale: 避免重复记录
- Priority: 高

**[BEHAVIOR.COLLECT.FILTER.2]** 系统应只采集用户真实操作（spontaneous事件），忽略程序发送的事件。
- Rationale: 只记录用户行为
- Priority: 高

**[BEHAVIOR.COLLECT.FILTER.3]** 系统应记录空白区域的点击（area_click），用于误操作分析。
- Rationale: 发现UI布局问题
- Priority: 中

**[BEHAVIOR.COLLECT.IGNORE.1]** 系统应自动忽略纯展示控件（QLabel、QFrame等）的点击。
- Rationale: 减少无用数据
- Priority: 中

**[BEHAVIOR.COLLECT.IGNORE.2]** 系统应支持通过API添加忽略控件。
- Rationale: 项目可自定义忽略规则
- Priority: 中

**[BEHAVIOR.COLLECT.GLOBAL.1]** 系统应在QApplication级别安装全局事件过滤器，捕获所有窗口的操作。
- Rationale: 支持多窗口应用
- Priority: 高

### 3.2 数据存储

#### 3.2.1 描述

将采集的数据存储到本地SQLite数据库，支持批量写入、版本迁移、数据清理。

#### 3.2.2 功能需求

**[BEHAVIOR.STORE.LOCATION.1]** 系统应将数据库文件存储在应用数据目录（QStandardPaths::AppLocalDataLocation）。
- Rationale: 符合系统规范
- Priority: 高

**[BEHAVIOR.STORE.BATCH.1]** 系统应采用批量写入策略，累积100条或定时（1秒）后写入数据库。
- Rationale: 减少IO，提升性能
- Priority: 高

**[BEHAVIOR.STORE.THREAD.1]** 系统应在工作线程执行数据库写入操作，不阻塞UI线程。
- Rationale: 保证UI流畅度
- Priority: 高

**[BEHAVIOR.STORE.VERSION.1]** 系统应在数据库中记录版本号。
- Rationale: 支持版本迁移
- Priority: 高

**[BEHAVIOR.STORE.MIGRATE.1]** 系统应在启动时检查数据库版本，执行必要的迁移脚本。
- Rationale: 升级时保留数据
- Priority: 高

**[BEHAVIOR.STORE.CLEAN.1]** 系统应自动清理超过180天的数据。
- Rationale: 控制数据库大小
- Priority: 中

### 3.3 会话管理

#### 3.3.1 描述

以应用生命周期为会话单位，记录会话信息。

#### 3.3.2 功能需求

**[BEHAVIOR.SESSION.DEFINE.1]** 系统应以应用启动为会话开始，应用关闭为会话结束。
- Rationale: 仪器仪表软件通常是单次运行
- Priority: 高

**[BEHAVIOR.SESSION.RECORD.1]** 系统应记录会话ID、开始时间、结束时间、时长、操作总数。
- Rationale: 支持会话分析
- Priority: 高

**[BEHAVIOR.SESSION.AUTO.1]** 系统应在初始化时自动开始会话，在关闭时自动结束会话。
- Rationale: 零配置
- Priority: 高

### 3.4 数据分析

#### 3.4.1 描述

提供多种内置分析器，支持按需分析、时间范围选择。

#### 3.4.2 功能需求

**[BEHAVIOR.ANALYZE.FREQUENCY.1]** 系统应提供操作频率分析器，统计各操作的调用次数和占比。
- Rationale: 了解用户最常用的操作
- Priority: 高

**[BEHAVIOR.ANALYZE.MODULE.1]** 系统应提供模块热度分析器，统计各模块的访问次数和占比。
- Rationale: 了解用户最常用的功能模块
- Priority: 高

**[BEHAVIOR.ANALYZE.TIME.1]** 系统应提供时段分布分析器，统计按小时/天的操作分布。
- Rationale: 了解用户使用时间规律
- Priority: 高

**[BEHAVIOR.ANALYZE.INPUT.1]** 系统应提供输入方式分析器，统计鼠标、触屏、快捷键的使用比例。
- Rationale: 了解用户操作习惯
- Priority: 高

**[BEHAVIOR.ANALYZE.HEATMAP.1]** 系统应提供屏幕热区分析器，统计主窗口的点击位置分布（10x10区域）。
- Rationale: 优化界面布局
- Priority: 高

**[BEHAVIOR.ANALYZE.HEATMAP.2]** 系统应只将主窗口的点击计入热区分析，对话框点击不计入。
- Rationale: 对话框位置不固定，热区分析无意义
- Priority: 高

**[BEHAVIOR.ANALYZE.DIALOG.1]** 系统应提供对话框统计分析器，统计各对话框的打开次数和平均保留时间。
- Rationale: 了解对话框使用情况
- Priority: 高

**[BEHAVIOR.ANALYZE.RANGE.1]** 系统应支持用户选择分析的时间范围。
- Rationale: 灵活分析
- Priority: 高

**[BEHAVIOR.ANALYZE.DEFAULT.1]** 系统应默认分析最近7天的数据。
- Rationale: 数据量适中，有统计意义
- Priority: 中

**[BEHAVIOR.ANALYZE.AGG.1]** 系统应对超过7天的时间范围使用预聚合表查询，7天及以内查询原始表。
- Rationale: 大范围查询原始表性能不可接受（100万条半年数据33秒→聚合210毫秒）
- Priority: 高

**[BEHAVIOR.ANALYZE.AGG.2]** 系统应自动按天和按小时聚合历史数据，存储到聚合表。
- Rationale: 预计算加速查询
- Priority: 高

**[BEHAVIOR.ANALYZE.AGG.3]** 系统应在后台工作线程执行聚合任务，不阻塞UI线程。
- Rationale: 保证UI流畅度
- Priority: 高

**[BEHAVIOR.ANALYZE.THREAD.1]** 系统应在工作线程执行分析查询，不阻塞UI线程。
- Rationale: 保证UI流畅度
- Priority: 高

### 3.5 可视化

#### 3.5.1 描述

提供基于Qwt的图表组件，展示分析结果。

#### 3.5.2 功能需求

**[BEHAVIOR.VIS.CHART.1]** 系统应提供柱状图组件，用于展示操作频率、模块热度等。
- Rationale: 直观展示数据
- Priority: 高

**[BEHAVIOR.VIS.CHART.2]** 系统应提供饼图组件，用于展示占比分布。
- Rationale: 直观展示占比
- Priority: 高

**[BEHAVIOR.VIS.CHART.3]** 系统应提供折线图组件，用于展示时段分布、趋势等。
- Rationale: 直观展示趋势
- Priority: 高

**[BEHAVIOR.VIS.CHART.4]** 系统应提供热力图组件，用于展示屏幕热区。
- Rationale: 直观展示点击分布
- Priority: 高

**[BEHAVIOR.VIS.TECH.1]** 系统应使用Qwt图表库实现图表组件。
- Rationale: 适合技术/科学应用
- Priority: 高

### 3.6 扩展机制

#### 3.6.1 描述

支持项目自定义分析器和图表。

#### 3.6.2 功能需求

**[BEHAVIOR.EXTEND.ANALYZER.1]** 系统应提供分析器基类，项目可继承实现自定义分析器。
- Rationale: 支持项目特定分析需求
- Priority: 高

**[BEHAVIOR.EXTEND.REGISTER.1]** 系统应提供分析器注册机制，项目可注册自定义分析器。
- Rationale: 扩展分析能力
- Priority: 高

**[BEHAVIOR.EXTEND.DATA.1]** 系统应提供原始数据导出接口，供自定义分析器使用。
- Rationale: 自定义分析需要原始数据
- Priority: 高

---

## 4. 数据需求

### 4.1 数据库表设计

#### 4.1.1 原始操作表（operations）

| 字段名 | 类型 | 说明 |
|--------|------|------|
| id | INTEGER | 主键 |
| session_id | TEXT | 会话ID |
| time | INTEGER | 时间戳（毫秒） |
| event_type | TEXT | 事件类型（mouse_click/touch_tap/shortcut/dialog_open/dialog_close/area_click） |
| control_class | TEXT | 控件类名 |
| control_name | TEXT | 控件objectName |
| control_text | TEXT | 控件显示文本 |
| control_path | TEXT | 控件完整路径 |
| action_name | TEXT | QAction名称（快捷键时） |
| key_sequence | TEXT | 按键组合（如 Ctrl+S，快捷键时） |
| window_class | TEXT | 窗口类名 |
| window_title | TEXT | 窗口标题 |
| window_path | TEXT | 窗口完整路径 |
| is_main_window | INTEGER | 是否主窗口（0/1） |
| screen_x | INTEGER | 屏幕X坐标 |
| screen_y | INTEGER | 屏幕Y坐标 |
| heat_region | INTEGER | 热区编号（0-99） |
| input_method | TEXT | 输入方式（mouse_click/touch_tap/shortcut/scroll/knob） |
| duration | INTEGER | 耗时（毫秒，对话框关闭时） |
| module | TEXT | 业务模块标识（用于模块热度分析） |

**索引：**
- idx_ops_time ON operations(time)
- idx_ops_session ON operations(session_id)
- idx_ops_event ON operations(event_type)
- idx_ops_window ON operations(window_class)
- idx_ops_mainwin ON operations(is_main_window)

#### 4.1.2 会话表（sessions）

| 字段名 | 类型 | 说明 |
|--------|------|------|
| id | TEXT | 会话ID（主键） |
| start_time | INTEGER | 开始时间（毫秒） |
| end_time | INTEGER | 结束时间（毫秒） |
| duration_seconds | INTEGER | 时长（秒） |
| operation_count | INTEGER | 操作总数 |

#### 4.1.3 元数据表（metadata）

| 字段名 | 类型 | 说明 |
|--------|------|------|
| key | TEXT | 键（主键） |
| value | TEXT | 值 |

**预置记录：**
- key="version", value="2.0"

#### 4.1.4 聚合表

系统维护6张聚合表，用于加速大时间范围查询（>7天时使用）。

**agg_operation_stats（操作频率聚合）**

| 字段名 | 类型 | 说明 |
|--------|------|------|
| id | INTEGER | 主键 |
| time_bucket | TEXT | 时间桶（日期，如2026-07-17） |
| granularity | TEXT | 粒度（hour/day） |
| action_key | TEXT | 操作键名 |
| action_type | TEXT | 操作类型 |
| count | INTEGER | 计数 |

**agg_module_stats（模块热度聚合）**

| 字段名 | 类型 | 说明 |
|--------|------|------|
| id | INTEGER | 主键 |
| time_bucket | TEXT | 时间桶 |
| granularity | TEXT | 粒度 |
| module_class | TEXT | 模块标识 |
| count | INTEGER | 计数 |

**agg_input_stats（输入方式聚合）**

| 字段名 | 类型 | 说明 |
|--------|------|------|
| id | INTEGER | 主键 |
| time_bucket | TEXT | 时间桶 |
| granularity | TEXT | 粒度 |
| input_method | TEXT | 输入方式 |
| count | INTEGER | 计数 |

**agg_heatmap_stats（热区聚合）**

| 字段名 | 类型 | 说明 |
|--------|------|------|
| id | INTEGER | 主键 |
| time_bucket | TEXT | 时间桶 |
| granularity | TEXT | 粒度 |
| heat_region | INTEGER | 热区编号（0-99） |
| count | INTEGER | 计数 |

**agg_dialog_stats（对话框统计聚合）**

| 字段名 | 类型 | 说明 |
|--------|------|------|
| id | INTEGER | 主键 |
| time_bucket | TEXT | 时间桶 |
| granularity | TEXT | 粒度 |
| dialog_class | TEXT | 对话框标识（module/windowTitle） |
| open_count | INTEGER | 打开次数 |
| total_duration | INTEGER | 总保留时长（毫秒） |
| avg_duration | INTEGER | 平均保留时长（毫秒） |

**agg_time_distribution（时段分布聚合）**

| 字段名 | 类型 | 说明 |
|--------|------|------|
| id | INTEGER | 主键 |
| date | TEXT | 日期 |
| hour | INTEGER | 小时（0-23） |
| count | INTEGER | 计数 |

### 4.2 数据字典

| 数据项 | 定义 | 格式 |
|--------|------|------|
| 会话ID | UUID格式的唯一标识 | 字符串，36字符 |
| 时间戳 | 自1970-01-01起的毫秒数 | 整数 |
| 事件类型 | 操作的类型 | 枚举：mouse_click, touch_tap, shortcut, dialog_open, dialog_close, area_click |
| 输入方式 | 操作的输入方式 | 枚举：mouse_click, touch_tap, shortcut, scroll, knob（derived为系统生成，不计入统计） |
| 热区编号 | 屏幕10x10区域编号 | 整数，0-99 |
| 控件路径 | 从根到控件的完整路径 | 字符串，如 "MainWindow/centralWidget/btn_ok" |

### 4.3 数据保留

- 原始操作数据保留时长：180天
- 聚合数据（按小时/按天）保留时长：180天，用于加速趋势与时段分布查询
- 清理策略：启动时检查并删除超期原始数据与超期聚合数据
- 聚合策略：后台工作线程自动按天/按小时聚合，聚合阈值 rangeDays > 7 时走聚合表查询
- 预估大小：~225MB原始数据 + ~13MB聚合数据（每天5000次操作）

---

## 5. 外部接口需求

### 5.1 用户接口

**[BEHAVIOR.UI.API.1]** 系统应提供以下初始化API：
```cpp
// 初始化（一行启动）
BehaviorAnalytics::init();

// 关闭
BehaviorAnalytics::shutdown();
```

**[BEHAVIOR.UI.API.2]** 系统应提供以下配置API：
```cpp
// 设置数据保留天数
BehaviorAnalytics::setRetentionDays(int days);

// 添加忽略控件
BehaviorAnalytics::addIgnoreControl(const QString& className);

// 设置批量写入大小
BehaviorAnalytics::setBatchSize(int size);

// 启用/禁用采集
BehaviorAnalytics::setEnabled(bool enabled);

// 设置数据库路径
BehaviorAnalytics::setDatabasePath(const QString& path);

// 设置当前业务模块标识
BehaviorAnalytics::setModule(const QString& module);
```

**[BEHAVIOR.UI.API.3]** 系统应提供以下分析API：
```cpp
// 获取分析器
BehaviorAnalyzer* analyzer = BehaviorAnalytics::analyzer();

// 执行分析（指定时间范围）
QVariantMap result = analyzer->analyzeFrequency(start, end);
QVariantMap result = analyzer->analyzeModule(start, end);
QVariantMap result = analyzer->analyzeTimeDistribution(start, end);
QVariantMap result = analyzer->analyzeInputMethod(start, end);
QVariantMap result = analyzer->analyzeHeatmap(start, end);
QVariantMap result = analyzer->analyzeDialog(start, end);
```

**[BEHAVIOR.UI.API.4]** 系统应提供以下扩展API：
```cpp
// 注册自定义分析器
BehaviorAnalytics::registerAnalyzer(const QString& name, AnalyzerBase* analyzer);

// 导出原始数据
QList<Operation> BehaviorAnalytics::exportRawData(const QDateTime& start, const QDateTime& end);
```

### 5.2 软件接口

| 接口 | 说明 |
|------|------|
| Qt事件系统 | 通过QApplication事件过滤器拦截事件 |
| SQLite | 数据存储 |
| Qwt | 图表绘制 |

---

## 6. 质量属性需求

### 6.1 性能需求

**[BEHAVIOR.PERF.COLLECT.1]** 单次事件采集延迟应小于1毫秒。
- Fit Criterion: 在标准PC上，10000次采集的平均延迟 < 1ms

**[BEHAVIOR.PERF.STORE.1]** 批量写入100条记录的延迟应小于10毫秒。
- Fit Criterion: 在工作线程执行，不阻塞UI

**[BEHAVIOR.PERF.QUERY.1]** 查询10万条记录的延迟应小于100毫秒。
- Fit Criterion: 在工作线程执行，使用索引优化

**[BEHAVIOR.PERF.QUERY.2]** 查询100万条记录（半年范围）通过聚合表应在500毫秒内完成。
- Fit Criterion: 实测100万条半年数据聚合查询210ms（160倍于原始表查询）

**[BEHAVIOR.PERF.AGG.1]** 全量聚合100万条历史数据应在10秒内完成。
- Fit Criterion: 实测8.6秒

**[BEHAVIOR.PERF.UI.1]** 采集过程不应影响UI流畅度（保持60fps）。
- Fit Criterion: 在持续采集过程中，UI动画无卡顿

**[BEHAVIOR.PERF.UI.2]** 分析窗口应即时显示，图表延迟加载。
- Fit Criterion: 窗口构造不阻塞，updateCharts通过QTimer::singleShot延迟100ms

**[BEHAVIOR.PERF.MEMORY.1]** 内存占用应小于10MB。
- Fit Criterion: 采集10万条记录后，内存增量 < 10MB（聚合查询不加载全量数据到内存）

### 6.2 可靠性需求

**[BEHAVIOR.RELIABLE.QUEUE.1]** 系统应保证采集队列不丢失数据。
- Fit Criterion: 应用正常关闭时，队列中所有数据都已写入数据库

**[BEHAVIOR.RELIABLE.CRASH.1]** 系统应处理应用异常关闭的情况。
- Fit Criterion: 下次启动时，数据库可正常打开，无损坏

### 6.3 可维护性需求

**[BEHAVIOR.MAINT.VERSION.1]** 数据库Schema变更应有版本迁移支持。
- Fit Criterion: 从v1.x升级到v2.x后，旧数据保留

**[BEHAVIOR.MAINT.CODE.1]** 采集层代码应保持简洁（< 500行）。
- Fit Criterion: 代码行数统计

### 6.4 可移植性需求

**[BEHAVIOR.PORT.PLATFORM.1]** 系统应支持Windows、Linux、ARM平台。
- Fit Criterion: 在三个平台编译通过，功能正常

**[BEHAVIOR.PORT.QT.1]** 系统应支持Qt 5.x和Qt 6.x。
- Fit Criterion: 在Qt 5.15和Qt 6.x编译通过

---

## 7. 约束

### 7.1 技术约束

| 约束 | 说明 |
|------|------|
| 语言 | C++17 |
| 框架 | Qt 5.x 或 Qt 6.x |
| 图表库 | Qwt 6.x |
| 数据库 | SQLite 3.x |
| 构建工具 | CMake 3.15+ |

### 7.2 设计约束

| 约束 | 说明 |
|------|------|
| 零侵入 | 业务代码无需修改，只需一行初始化 |
| 本地存储 | 不支持远程数据传输 |
| 集成方式 | 集成到ui_shared共享库 |

### 7.3 实现约束

| 约束 | 说明 |
|------|------|
| 命名空间 | ui_shared::behavior |
| 多线程 | 采集在主线程，存储/分析在工作线程 |
| 无配置文件 | 通过代码API配置 |

---

## 8. 需求追溯矩阵

| 需求ID | 验证方法 | 优先级 |
|--------|---------|--------|
| BEHAVIOR.COLLECT.INIT.1 | 代码审查 + 集成测试 | 高 |
| BEHAVIOR.COLLECT.EVENT.1-5 | 功能测试 | 高 |
| BEHAVIOR.COLLECT.FILTER.1-3 | 功能测试 | 高 |
| BEHAVIOR.COLLECT.IGNORE.1-2 | 功能测试 | 中 |
| BEHAVIOR.COLLECT.GLOBAL.1 | 功能测试 | 高 |
| BEHAVIOR.STORE.LOCATION.1 | 功能测试 | 高 |
| BEHAVIOR.STORE.BATCH.1 | 性能测试 | 高 |
| BEHAVIOR.STORE.THREAD.1 | 性能测试 | 高 |
| BEHAVIOR.STORE.VERSION.1 | 功能测试 | 高 |
| BEHAVIOR.STORE.MIGRATE.1 | 功能测试 | 高 |
| BEHAVIOR.STORE.CLEAN.1 | 功能测试 | 中 |
| BEHAVIOR.SESSION.DEFINE.1 | 功能测试 | 高 |
| BEHAVIOR.SESSION.RECORD.1 | 功能测试 | 高 |
| BEHAVIOR.SESSION.AUTO.1 | 功能测试 | 高 |
| BEHAVIOR.ANALYZE.FREQUENCY.1 | 功能测试 | 高 |
| BEHAVIOR.ANALYZE.MODULE.1 | 功能测试 | 高 |
| BEHAVIOR.ANALYZE.TIME.1 | 功能测试 | 高 |
| BEHAVIOR.ANALYZE.INPUT.1 | 功能测试 | 高 |
| BEHAVIOR.ANALYZE.HEATMAP.1-2 | 功能测试 | 高 |
| BEHAVIOR.ANALYZE.DIALOG.1 | 功能测试 | 高 |
| BEHAVIOR.ANALYZE.RANGE.1 | 功能测试 | 高 |
| BEHAVIOR.ANALYZE.DEFAULT.1 | 功能测试 | 中 |
| BEHAVIOR.ANALYZE.AGG.1-3 | 性能测试 | 高 |
| BEHAVIOR.ANALYZE.THREAD.1 | 性能测试 | 高 |
| BEHAVIOR.VIS.CHART.1-4 | 功能测试 | 高 |
| BEHAVIOR.VIS.TECH.1 | 代码审查 | 高 |
| BEHAVIOR.EXTEND.ANALYZER.1 | 功能测试 | 高 |
| BEHAVIOR.EXTEND.REGISTER.1 | 功能测试 | 高 |
| BEHAVIOR.EXTEND.DATA.1 | 功能测试 | 高 |
| BEHAVIOR.PERF.COLLECT.1 | 性能测试 | 高 |
| BEHAVIOR.PERF.STORE.1 | 性能测试 | 高 |
| BEHAVIOR.PERF.QUERY.1 | 性能测试 | 高 |
| BEHAVIOR.PERF.QUERY.2 | 性能测试 | 高 |
| BEHAVIOR.PERF.AGG.1 | 性能测试 | 高 |
| BEHAVIOR.PERF.UI.1 | 性能测试 | 高 |
| BEHAVIOR.PERF.UI.2 | 性能测试 | 高 |
| BEHAVIOR.PERF.MEMORY.1 | 性能测试 | 高 |
| BEHAVIOR.RELIABLE.QUEUE.1 | 功能测试 | 高 |
| BEHAVIOR.RELIABLE.CRASH.1 | 功能测试 | 高 |
| BEHAVIOR.MAINT.VERSION.1 | 功能测试 | 高 |
| BEHAVIOR.MAINT.CODE.1 | 代码审查 | 中 |
| BEHAVIOR.PORT.PLATFORM.1 | 兼容性测试 | 高 |
| BEHAVIOR.PORT.QT.1 | 兼容性测试 | 高 |

---

## 附录

### 附录A：事件类型说明

| 事件类型 | 触发条件 | 记录内容 | 状态 |
|---------|---------|---------|------|
| mouse_click | 鼠标点击控件 | 控件信息、坐标、热区 | ✅ 已实现 |
| touch_tap | 触屏点击控件 | 控件信息、坐标、热区 | ✅ 已实现 |
| shortcut | 快捷键触发 | QAction名称、按键组合 | ✅ 已实现 |
| dialog_open | 对话框显示 | 对话框类名、标题 | ✅ 已实现 |
| dialog_close | 对话框关闭 | 对话框类名、保留时长 | ✅ 已实现 |
| area_click | 点击空白区域 | 坐标、所在窗口 | ⚠️ 类型已定义，未实现采集 |

### 附录B：分析器输出格式

**操作频率分析：**
```json
{
  "total_operations": 1250,
  "top_actions": [
    {"action": "measure/start", "count": 156, "percentage": 12.5},
    {"action": "file/save", "count": 89, "percentage": 7.1}
  ]
}
```

**模块热度分析：**
```json
{
  "total_modules": 8,
  "modules": [
    {"module": "MeasurePanel", "count": 450, "percentage": 36.0},
    {"module": "DataView", "count": 280, "percentage": 22.4}
  ]
}
```

**输入方式分析：**
```json
{
  "mouse_click": {"count": 800, "percentage": 64.0},
  "touch_tap": {"count": 200, "percentage": 16.0},
  "shortcut": {"count": 250, "percentage": 20.0},
  "scroll": {"count": 50, "percentage": 4.0},
  "knob": {"count": 10, "percentage": 0.8}
}
```

**屏幕热区分析：**
```json
{
  "regions": [
    {"region": 45, "count": 120},
    {"region": 46, "count": 98},
    {"region": 55, "count": 85}
  ]
}
```

**对话框统计：**
```json
{
  "dialogs": [
    {"class": "settings/SettingsDialog", "open_count": 15, "avg_duration_ms": 8500},
    {"class": "file/ExportDialog", "open_count": 8, "avg_duration_ms": 3200}
  ]
}
```

---

**文档结束**

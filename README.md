# Behavior SDK

基于 Qt 的用户行为追踪与分析 SDK，用于采集、存储和分析应用程序中的用户交互行为。

## 功能特性

### 🎯 核心功能

- **行为采集** - 自动捕获用户交互事件（点击、键盘、触摸等）
- **数据存储** - 基于 SQLite 的高效数据存储
- **数据分析** - 多维度行为分析与可视化
- **聚合统计** - 支持时间窗口聚合，提升查询性能

### 📊 分析维度

- **操作频率分析** - 统计用户最常用的操作
- **模块热度分析** - 分析各功能模块的使用情况
- **输入方式分析** - 鼠标、键盘、触摸的使用比例
- **时间分布分析** - 用户活跃时段分布
- **点击热力图** - 主窗口点击位置可视化

### ✨ 特色功能

- **自动分析** - 分析弹窗打开时自动执行统计（新增）
- **实时日志** - 实时显示采集事件
- **灵活配置** - 可配置采集范围、采样率等参数
- **数据导出** - 支持 JSON 格式导出

## 系统要求

### 必需

- **Qt 5.15+** (Core, Gui, Widgets, Sql)
- **CMake 3.16+**
- **C++17** 编译器
- **Qwt 6.1+** (用于图表显示)

### 可选

- MinGW 8.1+ (Windows)
- GCC 9+ (Linux)
- Clang 10+ (macOS)

## 编译说明

### Windows (MinGW)

```bash
# 创建构建目录
mkdir build && cd build

# 配置项目
cmake .. -DCMAKE_PREFIX_PATH="D:/Qt/5.15.2/mingw81_64" -G "MinGW Makefiles"

# 编译
mingw32-make

# 运行 Demo
.\bin\behavior_demo.exe
```

### Linux

```bash
# 创建构建目录
mkdir build && cd build

# 配置项目
cmake .. -DCMAKE_PREFIX_PATH="/path/to/Qt/5.15.2/gcc_64"

# 编译
make

# 运行 Demo
./bin/behavior_demo
```

## 使用方法

### 1. 初始化 SDK

```cpp
#include "core/BehaviorAnalytics.h"

using namespace ui_shared::behavior;

// 初始化
BehaviorAnalytics::instance().init();

// 设置用户ID（可选）
BehaviorAnalytics::instance().setUserId("user_001");

// 开始采集
BehaviorAnalytics::instance().start();
```

### 2. 手动埋点

```cpp
// 记录自定义事件
BehaviorAnalytics::instance().trackEvent(
    "custom_action",
    "button_click",
    {{"button_name", "submit"}, {"form_id", "login_form"}}
);
```

### 3. 模块追踪

```cpp
// 进入模块
TRACK_MODULE_ENTER("SettingsDialog");

// 离开模块
TRACK_MODULE_LEAVE("SettingsDialog");
```

### 4. 数据分析

```cpp
auto* analyzer = BehaviorAnalytics::analyzer();

// 操作频率分析
auto freqResult = analyzer->analyzeFrequency(startTime, endTime);

// 模块热度分析
auto modResult = analyzer->analyzeModule(startTime, endTime);

// 时间分布分析
auto timeResult = analyzer->analyzeTime(startTime, endTime);

// 热力图分析
auto heatResult = analyzer->analyzeHeatmap(startTime, endTime);
```

### 5. 数据聚合

```cpp
// 手动触发聚合（建议定时执行）
auto& aggregator = Aggregator::instance();
aggregator.aggregateAll();
```

## 项目结构

```
behavior_sdk/
├── src/
│   ├── core/                  # 核心模块
│   │   ├── BehaviorAnalytics  # 主入口
│   │   ├── Config             # 配置管理
│   │   ├── Session            # 会话管理
│   │   └── Types              # 数据类型定义
│   │
│   ├── collector/             # 数据采集模块
│   │   ├── EventFilter        # Qt 事件过滤器
│   │   ├── EventProcessor     # 事件处理器
│   │   ├── ControlInspector   # 控件检查器
│   │   ├── DialogTracker      # 对话框追踪
│   │   └── ShortcutResolver   # 快捷键解析
│   │
│   ├── analyzer/              # 分析器模块
│   │   ├── FrequencyAnalyzer  # 操作频率分析
│   │   ├── ModuleAnalyzer     # 模块热度分析
│   │   ├── InputAnalyzer      # 输入方式分析
│   │   ├── TimeAnalyzer       # 时间分布分析
│   │   ├── HeatmapAnalyzer    # 热力图分析
│   │   └── DialogAnalyzer     # 对话框分析
│   │
│   └── storage/               # 存储模块
│       ├── Database           # 数据库管理
│       ├── Aggregator         # 数据聚合
│       ├── BatchWriter        # 批量写入
│       └── OperationQueue     # 操作队列
│
├── demo/                      # Demo 应用
│   ├── main.cpp               # 主入口
│   ├── mainwindow.cpp         # 主窗口
│   └── mainwindow.h           # 主窗口头文件
│
├── CMakeLists.txt             # 项目构建配置
└── README.md                  # 本文档
```

## 数据库结构

### 主要表

- **operations** - 原始操作记录
- **agg_operation_stats** - 操作统计聚合
- **agg_module_stats** - 模块统计聚合
- **agg_input_stats** - 输入方式统计聚合
- **agg_time_distribution** - 时间分布统计
- **agg_heatmap_stats** - 热力图统计聚合
- **agg_dialog_stats** - 对话框统计聚合

## 配置选项

可通过 `Config` 类配置以下参数：

```cpp
Config config;
config.enabled = true;              // 是否启用采集
config.sampleRate = 1.0;            // 采样率 (0.0-1.0)
config.storagePath = "tracking.db"; // 数据库路径
config.retentionDays = 30;          // 数据保留天数
config.batchSize = 100;             // 批量写入大小
```

## 性能优化

- **批量写入** - 使用队列批量写入数据库，减少 I/O 次数
- **异步处理** - 事件采集不阻塞主线程
- **数据聚合** - 定时聚合原始数据，提升查询性能
- **索引优化** - 关键字段建立索引，加速查询

## 更新日志

### v1.1.0 (2026-06-24)

- ✨ 新增：分析弹窗打开时自动执行统计
- 🎨 优化：改进分析弹窗 UI 样式
- 🐛 修复：时间转换使用本地时区

### v1.0.0 (Initial Release)

- 🎉 初始版本发布
- ✨ 基础行为采集功能
- ✨ 多维度数据分析
- ✨ 可视化图表展示
- ✨ SQLite 数据存储

## 许可证

本项目采用 MIT 许可证。详见 [LICENSE](LICENSE) 文件。

## 贡献指南

欢迎提交 Issue 和 Pull Request！

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 创建 Pull Request

## 联系方式

- **作者**: syble56
- **GitHub**: https://github.com/syble56/behavior

## 致谢

- [Qt Framework](https://www.qt.io/) - 跨平台应用框架
- [Qwt](https://qwt.sourceforge.io/) - Qt Widgets for Technical Applications
- [SQLite](https://www.sqlite.org/) - 嵌入式数据库引擎

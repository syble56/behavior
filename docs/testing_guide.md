# Behavior SDK 测试指导

## 1. 测试环境

### 1.1 前置条件
- Windows 10/11，Qt 5.15.2 (MinGW 64-bit)
- `behavior_demo.exe` 可正常启动
- 数据库文件可读写（默认路径或自定义路径）

### 1.2 模拟方式
| 输入方式 | 模拟方法 |
|---|---|
| 鼠标点击 | 鼠标左键点击控件 |
| 触摸点击 | 触屏设备点按（或 demo 中模拟） |
| 键盘快捷键 | Ctrl+S、Ctrl+C 等带修饰键的组合 |
| 独立按键 | Esc、Enter |
| 旋钮 | **Ctrl+F11** = 左旋，**Ctrl+F12** = 右旋 |

---

## 2. 事件采集测试

### 2.1 鼠标点击
| 编号 | 测试内容 | 操作步骤 | 预期结果 |
|---|---|---|---|
| M01 | 普通按钮点击 | 点击 demo 中的任意 QPushButton | 记录 event_type=mouse_click, input_method=mouse |
| M02 | 控件信息记录 | 点击已知控件 | control_class、control_name、control_text 非空 |
| M03 | 坐标记录 | 在不同位置点击 | screen_x、screen_y 反映实际点击位置 |
| M04 | 热区计算 | 在屏幕不同区域点击 | heat_region 为 0-99 的值 |
| M05 | 窗口信息 | 在主窗口和子窗口分别点击 | window_title、is_main_window 正确 |
| M06 | 去重 | 同一控件 50ms 内连续点击 | 只记录一次 |
| M07 | 忽略控件 | 配置 addIgnoreControl 后点击该控件 | 不记录 |

### 2.2 键盘快捷键
| 编号 | 测试内容 | 操作步骤 | 预期结果 |
|---|---|---|---|
| K01 | 带修饰键记录 | 按 Ctrl+S | 记录 event_type=shortcut, input_method=keyboard |
| K02 | action_name 解析 | 按 demo 中已绑定的快捷键 | action_name 显示对应 QAction 的文本 |
| K03 | 无修饰键不记录 | 单独按 A、B、C 等字母键 | 不记录为 shortcut |
| K04 | Esc 记录 | 按 Esc | 记录 event_type=key_press, action_name=escape |
| K05 | Enter 记录 | 按 Enter | 记录 event_type=key_press, action_name=enter |

### 2.3 旋钮
| 编号 | 测试内容 | 操作步骤 | 预期结果 |
|---|---|---|---|
| R01 | 左旋记录 | 按 Ctrl+F11 | 记录 event_type=knob, input_method=knob, action_name=knob_left |
| R02 | 右旋记录 | 按 Ctrl+F12 | 记录 event_type=knob, action_method=knob, action_name=knob_right |
| R03 | 不拦截按键 | 按 Ctrl+F11 | 应用层仍能收到该事件（不阻断） |
| R04 | 不误判其他快捷键 | 按 Ctrl+S、Ctrl+C | 记录为 shortcut，不是 knob |
| R05 | 连续旋转 | 快速按 Ctrl+F11 五次 | 5 条 knob 记录，无丢失 |
| R06 | 交替旋转 | 交替按 Ctrl+F11 / Ctrl+F12 | left/right 顺序正确 |
| R07 | key_sequence 记录 | 查看旋钮记录 | key_sequence 字段为 "Ctrl+F11" 或 "Ctrl+F12" |

### 2.4 对话框
| 编号 | 测试内容 | 操作步骤 | 预期结果 |
|---|---|---|---|
| D01 | 对话框打开 | 弹出任意 QDialog | 记录 event_type=dialog_open |
| D02 | 对话框关闭 | 关闭对话框 | 记录 event_type=dialog_close, duration 有值 |
| D03 | 耗时计算 | 打开对话框等待 3 秒后关闭 | duration ≈ 3000ms |
| D04 | 即时关闭 | 打开后立即关闭 | duration 接近 0 |
| D05 | 嵌套对话框 | A 弹出 B，关闭 B 再关闭 A | 两条 dialog_close，duration 各自正确 |

---

## 3. 数据存储测试

| 编号 | 测试内容 | 操作步骤 | 预期结果 |
|---|---|---|---|
| S01 | 数据库初始化 | 首次启动 | 自动创建 operations 表及相关索引 |
| S02 | 批量写入 | 产生超过 batchSize 条操作 | 批量写入数据库，无丢失 |
| S03 | 写入时机 | 产生少量操作后等待 | batchTimeoutMs 到期后自动写入 |
| S04 | 数据清理 | 设置 retentionDays=1，等待清理执行 | 超过 1 天的数据被删除 |
| S05 | 数据库迁移 | 从旧版本数据库启动 | Migrator 自动升级到 CURRENT_VERSION |
| S06 | 自定义路径 | setDatabasePath 后启动 | 数据库文件在指定路径创建 |
| S07 | 并发安全 | 多线程同时产生操作 | 无数据库锁死、数据损坏 |

---

## 4. 会话管理测试

| 编号 | 测试内容 | 操作步骤 | 预期结果 |
|---|---|---|---|
| C01 | 会话创建 | SDK 初始化 | 生成唯一 session_id |
| C02 | 会话关联 | 产生若干操作 | 所有记录的 session_id 一致 |
| C03 | 会话时长 | 操作一段时间后关闭 | session 记录的 duration_seconds 合理 |
| C04 | 多次启动 | 关闭 demo 后重新启动 | 新 session_id，与之前不同 |

---

## 5. 分析功能测试

### 5.1 频率分析（Frequency）
| 编号 | 测试内容 | 预期结果 |
|---|---|---|
| F01 | 点击频率统计 | 各控件点击次数正确排序 |
| F02 | 时间范围过滤 | 只统计指定时间段内的操作 |
| F03 | 空数据 | 无操作时查询 | 返回空结果，不崩溃 |

### 5.2 模块分析（Module）
| 编号 | 测试内容 | 预期结果 |
|---|---|---|
| P01 | 模块分布 | 各 module 操作次数正确 |
| P02 | 未设置模块 | module 为空的操作归为"未分类"或空 |

### 5.3 输入方式分析（Input Method）
| 编号 | 测试内容 | 预期结果 |
|---|---|---|
| I01 | 分类统计 | Mouse / Touch / Keyboard / Knob 各类数量正确 |
| I02 | 百分比 | 各类百分比之和 = 100% |
| I03 | 饼图显示 | 分析面板 Input Method 页饼图正确渲染 |
| I04 | 仅有旋钮 | 只做旋钮操作后查询 | Knob = 100% |

### 5.4 时间分布分析（Time）
| 编号 | 测试内容 | 预期结果 |
|---|---|---|
| T01 | 时段分布 | 操作按小时/天正确分桶 |
| T02 | 跨天查询 | 查询多天数据 | 每天数据独立统计 |

### 5.5 热力图分析（Heatmap）
| 编号 | 测试内容 | 预期结果 |
|---|---|---|
| H01 | 热区统计 | 10×10 网格各区域计数正确 |
| H02 | 颜色映射 | 高频区域颜色更深 |
| H03 | 边界点击 | 点击屏幕边缘 | 归入正确的边界热区（0 或 99） |

### 5.6 对话框分析（Dialog）
| 编号 | 测试内容 | 预期结果 |
|---|---|---|
| DA01 | 打开次数 | 各对话框 open_count 正确 |
| DA02 | 平均耗时 | avg_duration 计算正确 |
| DA03 | 散点图 | 分析面板散点图正确渲染 |
| DA04 | 表格显示 | Dialog Analysis 表格数据正确 |

---

## 6. 聚合测试

| 编号 | 测试内容 | 操作步骤 | 预期结果 |
|---|---|---|---|
| AG01 | 日聚合 | 执行聚合 > 7 天范围查询 | 走 agg_* 统计表，结果与原始数据一致 |
| AG02 | 原始查询 | 查询 ≤ 7 天范围 | 直接查 operations 表 |
| AG03 | 聚合完整性 | 聚合后查询 | operations、modules、inputs、heatmap、dialog、time 各表数据一致 |
| AG04 | 增量聚合 | 新增数据后重新聚合 | 只聚合新增部分，不重复 |

---

## 7. 配置测试

| 编号 | 测试内容 | 操作步骤 | 预期结果 |
|---|---|---|---|
| CF01 | 启用/禁用 | setEnabled(false) 后操作 | 不记录任何操作 |
| CF02 | 重新启用 | setEnabled(true) 后操作 | 恢复记录 |
| CF03 | 批量大小 | setBatchSize(10) | 每 10 条触发一次写入 |
| CF04 | 保留天数 | setRetentionDays(7) | 7 天前数据被清理 |
| CF05 | 忽略控件 | addIgnoreControl("QPushButton") | 该类型控件不记录 |
| CF06 | 移除忽略 | removeIgnoreControl("QPushButton") | 恢复记录 |

---

## 8. 稳定性测试

| 编号 | 测试内容 | 操作步骤 | 预期结果 |
|---|---|---|---|
| ST01 | 长时间运行 | 持续操作 8 小时 | 无内存泄漏、无崩溃 |
| ST02 | 高频操作 | 快速点击 1000 次 | 无丢失、无崩溃 |
| ST03 | 数据库异常 | 数据库文件被占用/锁定 | 优雅处理，不崩溃 |
| ST04 | 关闭流程 | shutdown() | 所有缓冲数据写入数据库，线程正常退出 |

---

## 9. 验证方式

### 9.1 demo 界面
- **Operations 页**：实时查看操作记录列表
- **Modules 页**：模块分布统计
- **Input Method 页**：输入方式饼图
- **Time 页**：时间分布图
- **Heatmap 页**：热力图
- **Dialog Analysis 页**：对话框统计表格 + 散点图

### 9.2 数据库直接查询
```sql
-- 查看最近操作
SELECT id, session_id, datetime(time/1000, 'unixepoch', 'localtime') as t,
       event_type, input_method, action_name, control_name, window_title
FROM operations ORDER BY time DESC LIMIT 20;

-- 统计各事件类型数量
SELECT event_type, COUNT(*) FROM operations GROUP BY event_type;

-- 统计各输入方式数量
SELECT input_method, COUNT(*) FROM operations GROUP BY input_method;

-- 查看旋钮记录
SELECT * FROM operations WHERE event_type = 'knob' ORDER BY time DESC;

-- 查看对话框记录
SELECT event_type, window_title, duration 
FROM operations WHERE event_type LIKE 'dialog%' ORDER BY time DESC;
```

### 9.3 测试工具
- `query_op_detail.exe`：查询操作详情
- `check_agg.exe`：检查聚合数据一致性
- `check_dialog.exe`：检查对话框统计
- `stress_test.exe`：压力测试

---

## 10. 测试通过标准

- 所有用例预期结果与实际结果一致
- 无崩溃、无数据丢失、无数据库损坏
- demo 各页面数据展示正确
- 性能：单次操作记录延迟 < 1ms，批量写入不影响 UI 响应

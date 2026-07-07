# input_method 改动方案

## 背景

当前 `dialog_open` / `dialog_close` 等衍生事件硬编码 `InputMethod::Mouse`，导致输入方式统计虚增。

**实际问题：** 用户点击按钮打开弹窗再关闭，实际 2 次鼠标输入，但产生 4 条 `input_method = mouse` 的记录：

| 步骤 | 事件 | 当前 input_method |
|------|------|-------------------|
| 1 | mouse_click（点击按钮） | mouse ✅ |
| 2 | dialog_open（弹窗打开） | mouse ❌ 衍生 |
| 3 | mouse_click（点击关闭） | mouse ✅ |
| 4 | dialog_close（弹窗关闭） | mouse ❌ 衍生 |

## 决策

**方案 1：衍生事件不记 input_method**

- 只有直接物理输入才记 `input_method`
- 衍生事件（dialog_open/dialog_close、选件-测量项开关等）记为 `derived`
- 统计时 `WHERE input_method != 'derived'` 即可拿到真实输入次数

## 改动范围

### 1. `src/core/types.h` — 扩展 InputMethod 枚举

**当前：**
```cpp
enum class InputMethod {
    Mouse,
    Touch,
    Keyboard
};
```

**改为：**
```cpp
enum class InputMethod {
    Mouse,      // 鼠标点击/拖拽
    Touch,      // 触摸点击/滑动
    Keyboard,   // 键盘按键
    Scroll,     // 滚动（鼠标滑轮、触控板滚动）
    Knob,       // 旋钮
    Derived     // 衍生事件（dialog_open/close、选件开关等）
};
```

**同步修改字符串转换函数：**
```cpp
inline const char* inputMethodToString(InputMethod m) {
    switch (m) {
        case InputMethod::Mouse:    return "mouse";
        case InputMethod::Touch:    return "touch";
        case InputMethod::Keyboard: return "keyboard";
        case InputMethod::Scroll:   return "scroll";
        case InputMethod::Knob:     return "knob";
        case InputMethod::Derived:  return "derived";
    }
    return "derived";
}

inline InputMethod stringToInputMethod(const QString& s) {
    if (s == "mouse")    return InputMethod::Mouse;
    if (s == "touch")    return InputMethod::Touch;
    if (s == "keyboard") return InputMethod::Keyboard;
    if (s == "scroll")   return InputMethod::Scroll;
    if (s == "knob")     return InputMethod::Knob;
    if (s == "derived")  return InputMethod::Derived;
    return InputMethod::Derived;  // 未知值降级为 derived
}
```

### 2. `src/collector/event_processor.cpp` — 衍生事件改用 Derived

**processDialogOpen：**
```cpp
// 改前
Operation op = createOperation(qobject_cast<QWidget*>(dialog),
                               EventType::DialogOpen, InputMethod::Mouse);

// 改后
Operation op = createOperation(qobject_cast<QWidget*>(dialog),
                               EventType::DialogOpen, InputMethod::Derived);
```

**processDialogClose：**
```cpp
// 改前
Operation op = createOperation(qobject_cast<QWidget*>(dialog),
                               EventType::DialogClose, InputMethod::Mouse);

// 改后
Operation op = createOperation(qobject_cast<QWidget*>(dialog),
                               EventType::DialogClose, InputMethod::Derived);
```

### 3. `src/analyzer/input_analyzer.cpp` — 统计排除 Derived，新增 Scroll/Knob

**改为：**
```cpp
AnalysisResult InputAnalyzer::analyze(const QList<Operation>& ops) {
    AnalysisResult r;
    if (ops.isEmpty()) { r.error = QStringLiteral("数据不足"); return r; }
    
    int mouse = 0, touch = 0, keyboard = 0, scroll = 0, knob = 0;
    for (const auto& op : ops) {
        switch (op.inputMethod) {
            case InputMethod::Mouse:    mouse++; break;
            case InputMethod::Touch:    touch++; break;
            case InputMethod::Keyboard: keyboard++; break;
            case InputMethod::Scroll:   scroll++; break;
            case InputMethod::Knob:     knob++; break;
            case InputMethod::Derived:  break;  // 衡生事件不计入输入统计
        }
    }
    int total = mouse + touch + keyboard + scroll + knob;
    auto pct = [total](int c){ return (total > 0) ? (c * 100.0 / total) : 0.0; };

    QVariantMap m, t, k, s, kn;
    m["count"] = mouse;     m["percentage"] = pct(mouse);
    t["count"] = touch;     t["percentage"] = pct(touch);
    k["count"] = keyboard;  k["percentage"] = pct(keyboard);
    s["count"] = scroll;    s["percentage"] = pct(scroll);
    kn["count"] = knob;     kn["percentage"] = pct(knob);
    
    r.data["total"] = total;
    r.data["mouse"] = m;
    r.data["touch"] = t;
    r.data["keyboard"] = k;
    r.data["scroll"] = s;
    r.data["knob"] = kn;
    r.success = true;
    return r;
}
```

### 4. 数据库迁移 — `src/storage/migrator.cpp`

如果数据库中已存有 `input_method = 'mouse'` 的衍生事件记录，需要考虑是否做数据迁移。

**建议：** 新数据直接用 `derived`，旧数据可选跑一次迁移脚本：
```sql
UPDATE operations SET input_method = 'derived'
WHERE event_type IN ('dialog_open', 'dialog_close');
```

如果不迁移，分析端在统计时排除 `event_type IN ('dialog_open', 'dialog_close')` 也可以，但不如数据层干净。

## 预留扩展

未来可能新增的衍生事件类型（同样使用 `InputMethod::Derived`）：

| 事件类型 | 说明 |
|---|---|
| menu_open / menu_close | 菜单开关 |
| tab_switch | tab 切换 |
| panel_expand / panel_collapse | 面板展开/折叠 |
| option_toggle_on / option_toggle_off | 选件-测量项开关 |

## 改动文件清单

| 文件 | 改动内容 |
|------|---------|
| `src/core/types.h` | InputMethod 枚举新增 Scroll/Knob/Derived + 字符串转换函数 |
| `src/collector/event_processor.cpp` | processDialogOpen/Close 改用 Derived |
| `src/analyzer/input_analyzer.cpp` | 统计排除 Derived，新增 Scroll/Knob 计数 |
| `src/storage/migrator.cpp` | （可选）旧数据迁移脚本 |

## 影响评估

- **破坏性：** 低。新增枚举值不影响已有数据，`stringToInputMethod` 对未知值降级为 Derived
- **分析影响：** InputAnalyzer 输出结果中 total 会下降（不再包含衍生事件），这是正确行为
- **下游消费方：** 如果有其他代码硬编码了 `InputMethod::Mouse` 的三选一逻辑，需要同步更新

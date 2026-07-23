# behavior_sdk 静态库

set(BEHAVIOR_SOURCES
    core/config.cpp
    core/session.cpp
    core/behavior_analytics.cpp

    collector/event_filter.cpp
    collector/event_processor.cpp
    collector/control_inspector.cpp
    collector/shortcut_resolver.cpp
    collector/knob_resolver.cpp
    collector/dialog_tracker.cpp

    storage/database.cpp
    storage/operation_queue.cpp
    storage/batch_writer.cpp
    storage/cleaner.cpp
    storage/migrator.cpp
    storage/aggregator.cpp
    storage/aggregation_scheduler.cpp

    analyzer/analyzer_registry.cpp
    analyzer/frequency_analyzer.cpp
    analyzer/module_analyzer.cpp
    analyzer/time_analyzer.cpp
    analyzer/input_analyzer.cpp
    analyzer/heatmap_analyzer.cpp
    analyzer/dialog_analyzer.cpp
    analyzer/behavior_analyzer.cpp
)

set(BEHAVIOR_HEADERS
    core/types.h
    core/config.h
    core/session.h
    core/behavior_analytics.h

    collector/event_filter.h
    collector/event_processor.h
    collector/control_inspector.h
    collector/shortcut_resolver.h
    collector/knob_resolver.h
    collector/dialog_tracker.h

    storage/database.h
    storage/operation_queue.h
    storage/batch_writer.h
    storage/cleaner.h
    storage/migrator.h
    storage/aggregator.h
    storage/aggregation_scheduler.h

    analyzer/analyzer_base.h
    analyzer/analyzer_registry.h
    analyzer/frequency_analyzer.h
    analyzer/module_analyzer.h
    analyzer/time_analyzer.h
    analyzer/input_analyzer.h
    analyzer/heatmap_analyzer.h
    analyzer/dialog_analyzer.h
    analyzer/behavior_analyzer.h
)

# 显式 MOC 处理（含 Q_OBJECT 的头文件），避免 AUTOMOC 在 MinGW 上的 autogen 问题
set(BEHAVIOR_MOC_HEADERS
    core/behavior_analytics.h
    core/session.h
    collector/event_filter.h
    collector/event_processor.h
    collector/knob_resolver.h
    storage/batch_writer.h
    storage/cleaner.h
    storage/aggregator.h
    storage/aggregation_scheduler.h
    analyzer/behavior_analyzer.h
)

if(QT_VER EQUAL 6)
    qt6_wrap_cpp(BEHAVIOR_MOC_SRCS ${BEHAVIOR_MOC_HEADERS})
else()
    qt5_wrap_cpp(BEHAVIOR_MOC_SRCS ${BEHAVIOR_MOC_HEADERS})
endif()

add_library(behavior_sdk STATIC
    ${BEHAVIOR_SOURCES}
    ${BEHAVIOR_HEADERS}
    ${BEHAVIOR_MOC_SRCS}
)

target_include_directories(behavior_sdk
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/collector
        ${CMAKE_CURRENT_SOURCE_DIR}/storage
        ${CMAKE_CURRENT_SOURCE_DIR}/analyzer
)

target_link_libraries(behavior_sdk
    PUBLIC
        ${QT_PREFIX}::Core
        ${QT_PREFIX}::Gui
        ${QT_PREFIX}::Widgets
        ${QT_PREFIX}::Sql
)

# Windows 下 SQLite 通常由 Qt 的 QSqlDatabase 驱动提供（需 qt5-sql sqlite 插件）

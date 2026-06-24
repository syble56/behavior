# behavior_sdk 静态库

set(BEHAVIOR_SOURCES
    core/Config.cpp
    core/Session.cpp
    core/BehaviorAnalytics.cpp

    collector/EventFilter.cpp
    collector/EventProcessor.cpp
    collector/ControlInspector.cpp
    collector/ShortcutResolver.cpp
    collector/DialogTracker.cpp

    storage/Database.cpp
    storage/OperationQueue.cpp
    storage/BatchWriter.cpp
    storage/Migrator.cpp
    storage/Aggregator.cpp
    storage/AggregationScheduler.cpp

    analyzer/AnalyzerRegistry.cpp
    analyzer/FrequencyAnalyzer.cpp
    analyzer/ModuleAnalyzer.cpp
    analyzer/TimeAnalyzer.cpp
    analyzer/InputAnalyzer.cpp
    analyzer/HeatmapAnalyzer.cpp
    analyzer/DialogAnalyzer.cpp
    analyzer/BehaviorAnalyzer.cpp
)

set(BEHAVIOR_HEADERS
    core/Types.h
    core/Config.h
    core/Session.h
    core/BehaviorAnalytics.h

    collector/EventFilter.h
    collector/EventProcessor.h
    collector/ControlInspector.h
    collector/ShortcutResolver.h
    collector/DialogTracker.h

    storage/Database.h
    storage/OperationQueue.h
    storage/BatchWriter.h
    storage/Migrator.h
    storage/Aggregator.h
    storage/AggregationScheduler.h

    analyzer/AnalyzerBase.h
    analyzer/AnalyzerRegistry.h
    analyzer/FrequencyAnalyzer.h
    analyzer/ModuleAnalyzer.h
    analyzer/TimeAnalyzer.h
    analyzer/InputAnalyzer.h
    analyzer/HeatmapAnalyzer.h
    analyzer/DialogAnalyzer.h
    analyzer/BehaviorAnalyzer.h
)

# 显式 MOC 处理（含 Q_OBJECT 的头文件），避免 AUTOMOC 在 MinGW 上的 autogen 问题
set(BEHAVIOR_MOC_HEADERS
    core/BehaviorAnalytics.h
    core/Session.h
    collector/EventFilter.h
    collector/EventProcessor.h
    storage/BatchWriter.h
    storage/Aggregator.h
    storage/AggregationScheduler.h
    analyzer/BehaviorAnalyzer.h
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

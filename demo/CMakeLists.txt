set(DEMO_SOURCES main.cpp mainwindow.cpp)
set(DEMO_MOC_HEADERS mainwindow.h)

if(QT_VER EQUAL 6)
    qt6_wrap_cpp(DEMO_MOC_SRCS ${DEMO_MOC_HEADERS})
else()
    qt5_wrap_cpp(DEMO_MOC_SRCS ${DEMO_MOC_HEADERS})
endif()

set_source_files_properties(mainwindow.cpp PROPERTIES AUTOMOC ON)

add_executable(behavior_demo ${DEMO_SOURCES} ${DEMO_MOC_SRCS})

set_target_properties(behavior_demo PROPERTIES AUTOMOC ON)

target_include_directories(behavior_demo PRIVATE ${QWT_INCLUDE_DIR})

target_link_libraries(behavior_demo
    PRIVATE
        behavior_sdk
        ${QT_PREFIX}::Core
        ${QT_PREFIX}::Gui
        ${QT_PREFIX}::Widgets
        ${QT_PREFIX}::Sql
        ${QWT_LIBRARY}
)

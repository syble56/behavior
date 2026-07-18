set(DEMO_SOURCES
    main.cpp
    mainwindow.cpp
    chart_widgets.cpp
    settings_dialog.cpp
    analysis_dialog.cpp
    operations_tab.cpp
    modules_tab.cpp
    input_tab.cpp
    time_tab.cpp
    heatmap_tab.cpp
    dialog_tab.cpp
    distance_tab.cpp
    date_picker/date_field.cpp
    date_picker/calendar_picker.cpp
    date_picker/dropdown_popup.cpp
)
set(DEMO_MOC_HEADERS
    mainwindow.h
    chart_widgets.h
    settings_dialog.h
    analysis_dialog.h
    operations_tab.h
    modules_tab.h
    input_tab.h
    time_tab.h
    heatmap_tab.h
    dialog_tab.h
    distance_tab.h
    date_picker/date_field.h
    date_picker/calendar_picker.h
    date_picker/dropdown_popup.h
)

if(QT_VER EQUAL 6)
    qt6_wrap_cpp(DEMO_MOC_SRCS ${DEMO_MOC_HEADERS})
else()
    qt5_wrap_cpp(DEMO_MOC_SRCS ${DEMO_MOC_HEADERS})
endif()

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

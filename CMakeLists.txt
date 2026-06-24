cmake_minimum_required(VERSION 3.15)
project(behavior_sdk VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# --- Qt: 优先 Qt6，回退 Qt5 ---
find_package(Qt6 QUIET COMPONENTS Core Gui Widgets Sql)
if(Qt6_FOUND)
    set(QT_VER 6)
    message(STATUS "Behavior SDK: using Qt6")
else()
    find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Sql)
    set(QT_VER 5)
    message(STATUS "Behavior SDK: using Qt5")
endif()

set(QT_PREFIX "Qt${QT_VER}")

# --- Qwt ---
set(QWT_DIR "D:/syble/qwt-6.3.0/qwt-6.3.0")
set(QWT_INCLUDE_DIR "${QWT_DIR}/src")
set(QWT_LIBRARY "${QWT_DIR}/lib/libqwt.a")
message(STATUS "Qwt: ${QWT_DIR}")

# --- 输出目录 ---
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(src)

# --- Demo ---
option(BEHAVIOR_BUILD_DEMO "Build demo app" ON)
if(BEHAVIOR_BUILD_DEMO)
    add_subdirectory(demo)
endif()

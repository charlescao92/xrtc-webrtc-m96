cmake_minimum_required(VERSION 3.5)

project(qtdemo VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        src/main.cpp
        src/mainwindow.cpp
        src/mainwindow.h
        src/mainwindow.ui
        src/YUVOpenGLWidget.cpp
        src/YUVOpenGLWidget.h
        src/defs.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(qtdemo
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET qtdemo APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(qtdemo SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(qtdemo
            ${PROJECT_SOURCES}
         )
    endif()
endif()

include_directories(
    ${KRTC_DIR}
    ${KRTC_DIR}/examples/qtdemo
)

link_directories(
    ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
)

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    message(STATUS "Current platform: Windows")

    target_link_libraries(qtdemo PRIVATE Qt${QT_VERSION_MAJOR}::Widgets xrtc)

    set_target_properties(qtdemo PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER charlescao92.cn
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
        LINK_FLAGS "/SUBSYSTEM:CONSOLE /SAFESEH:NO"
    )

elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    #sudo ln -s /lib/x86_64-linux-gnu/libglib-2.0.so.0 /lib/x86_64-linux-gnu/libglib-2.0.so
    #sudo ln -s /lib/x86_64-linux-gnu/libgio-2.0.so.0 /lib/x86_64-linux-gnu/libgio-2.0.so
    #sudo ln -s /usr/lib/x86_64-linux-gnu/libXrandr.so.2 /usr/lib/x86_64-linux-gnu/libXrandr.so
    #sudo ln -s /usr/lib/x86_64-linux-gnu/libXtst.so.6 /usr/lib/x86_64-linux-gnu/libXtst.so
    #sudo ln -s /usr/lib/x86_64-linux-gnu/libXdamage.so.1 /usr/lib/x86_64-linux-gnu/libXdamage.so
    #sudo ln -s /usr/lib/x86_64-linux-gnu/libXfixes.so.3 /usr/lib/x86_64-linux-gnu/libXfixes.so
    #sudo ln -s /usr/lib/x86_64-linux-gnu/libXcomposite.so.1 /usr/lib/x86_64-linux-gnu/libXcomposite.so
    #sudo ln -s /usr/lib/x86_64-linux-gnu/libgbm.so.1 /usr/lib/x86_64-linux-gnu/libgbm.so
    target_link_libraries(qtdemo PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libkrtc.so
        -lglib-2.0
        -lgio-2.0
        -lXrandr
        -lXtst
        -lXdamage
        -lXfixes
        -lXcomposite
        -lgbm
        -latomic
        -lssl
        -lcrypto
        -lpthread
        -ldl
        )

    set_target_properties(qtdemo PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER charlescao92.cn
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
    )
endif()


if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(qtdemo)
endif()

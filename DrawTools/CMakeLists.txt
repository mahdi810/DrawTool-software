cmake_minimum_required(VERSION 3.16)
project(DrawTools VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)

add_executable(DrawTools
    main.cpp
    mainwindow.h         mainwindow.cpp
    diagramscene.h       diagramscene.cpp
    diagramview.h        diagramview.cpp
    arrowitem.h          arrowitem.cpp
    resizablepixmapitem.h resizablepixmapitem.cpp
    resizableequationitem.h resizableequationitem.cpp
    diagramrectitem.h    diagramrectitem.cpp
    filemanager.h        filemanager.cpp
    propertiespanel.h    propertiespanel.cpp
    symbollibrary.h      symbollibrary.cpp
    commands.h
)

target_link_libraries(DrawTools PRIVATE Qt6::Widgets Qt6::Svg)
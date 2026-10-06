# SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.16)

project(cutefish-video-player VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CTest)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 OpenGL DBus LinguistTools ShaderTools)
find_package(CutefishFramework REQUIRED COMPONENTS Application)
find_package(PkgConfig REQUIRED)
pkg_check_modules(Mpv REQUIRED IMPORTED_TARGET mpv>=2.0)

add_library(player-backend STATIC
    src/application.cpp
    src/mpvnode.cpp
    src/playersettings.cpp
    src/playlist.cpp
    src/resumestore.cpp
    src/sleepinhibitor.cpp
    src/thumbnailer.cpp
    src/tracks.cpp
)
target_include_directories(player-backend PUBLIC src)
target_link_libraries(player-backend PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::DBus
    PkgConfig::Mpv
)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/mpvitem.cpp
    resources.qrc
)
target_compile_definitions(${PROJECT_NAME} PRIVATE VIDEO_PLAYER_VERSION="${PROJECT_VERSION}")
qt_add_shaders(${PROJECT_NAME} "player_shaders"
    PREFIX "/"
    FILES
    shaders/glass.frag
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    player-backend
    Cutefish::Application
    Qt6::OpenGL
    Qt6::QuickControls2
)

file(GLOB TS_FILES translations/*.ts)
qt_add_translation(QM_FILES ${TS_FILES})
add_custom_target(translations ALL DEPENDS ${QM_FILES} SOURCES ${TS_FILES})

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES cutefish-video-player.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_DATADIR}/cutefish-video-player/translations)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

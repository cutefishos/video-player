# SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(tst_backend tst_backend.cpp)
target_link_libraries(tst_backend PRIVATE player-backend Qt6::Test)
add_test(NAME video_player_backend COMMAND tst_backend)

add_executable(tst_media
    tst_media.cpp
    ${PROJECT_SOURCE_DIR}/src/mpvitem.cpp
)
target_link_libraries(tst_media PRIVATE player-backend Qt6::OpenGL Qt6::Test)
add_test(NAME video_player_media COMMAND tst_media -platform offscreen)

add_executable(tst_ui
    tst_ui.cpp
    ${PROJECT_SOURCE_DIR}/src/mpvitem.cpp
    ${PROJECT_SOURCE_DIR}/resources.qrc
)
qt_add_shaders(tst_ui "tst_ui_shaders"
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}"
    FILES
    ${PROJECT_SOURCE_DIR}/shaders/glass.frag
)
target_link_libraries(tst_ui PRIVATE player-backend Cutefish::Application
    Qt6::OpenGL Qt6::Quick Qt6::QuickControls2 Qt6::Test)
add_test(NAME video_player_ui COMMAND tst_ui -platform offscreen)

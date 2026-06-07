cmake_minimum_required(VERSION 3.16)
project(MacDockShell VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Network)

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

qt_add_executable(MacDockShell
    src/MacDockShell/main.cpp
    src/MacDockShell/TaskbarController.cpp
    src/MacDockShell/TaskbarController.h
    src/MacDockShell/DockModel.cpp
    src/MacDockShell/DockModel.h
    src/MacDockShell/AppBarController.cpp
    src/MacDockShell/AppBarController.h
    src/MacDockShell/PinnedTaskbarResolver.cpp
    src/MacDockShell/PinnedTaskbarResolver.h
    src/MacDockShell/WindowEffects.cpp
    src/MacDockShell/WindowEffects.h
    src/MacDockShell/HoverTracker.cpp
    src/MacDockShell/HoverTracker.h
    src/MacDockShell/MacCursor.cpp
    src/MacDockShell/MacCursor.h
)

qt_add_resources(MacDockShell macdockshell_resources
    PREFIX "/"
    FILES
        src/MacDockShell/qml/TopBarWindow.qml
        src/MacDockShell/qml/DockWindow.qml
        src/MacDockShell/qml/ControlWindow.qml
        src/MacDockShell/qml/components/StatusArea.qml
        src/MacDockShell/qml/components/ControlCenterPanel.qml
        src/MacDockShell/qml/components/MacToggle.qml
        src/MacDockShell/qml/components/qmldir
        src/MacDockShell/qml/apple_logo.svg
        src/MacDockShell/qml/apple_logo_white.svg
        src/MacDockShell/icons/finder_macos.png
        src/MacDockShell/cursors/100/arrow.cur
        src/MacDockShell/cursors/100/pointing_hand.cur
        src/MacDockShell/cursors/100/closed_hand.cur
        src/MacDockShell/cursors/100/forbidden.cur
        src/MacDockShell/cursors/150/arrow.cur
        src/MacDockShell/cursors/150/pointing_hand.cur
        src/MacDockShell/cursors/150/closed_hand.cur
        src/MacDockShell/cursors/150/forbidden.cur
        src/MacDockShell/cursors/100/text.cur
        src/MacDockShell/cursors/100/horizontal_resize.cur
        src/MacDockShell/cursors/100/vertical_resize.cur
        src/MacDockShell/cursors/100/diagonal_resize_1.cur
        src/MacDockShell/cursors/100/diagonal_resize_2.cur
        src/MacDockShell/cursors/100/help.cur
        src/MacDockShell/cursors/100/precision.cur
        src/MacDockShell/cursors/100/busy.ani
        src/MacDockShell/cursors/100/working.ani
        src/MacDockShell/cursors/150/text.cur
        src/MacDockShell/cursors/150/horizontal_resize.cur
        src/MacDockShell/cursors/150/vertical_resize.cur
        src/MacDockShell/cursors/150/diagonal_resize_1.cur
        src/MacDockShell/cursors/150/diagonal_resize_2.cur
        src/MacDockShell/cursors/150/help.cur
        src/MacDockShell/cursors/150/precision.cur
        src/MacDockShell/cursors/150/busy.ani
        src/MacDockShell/cursors/150/working.ani
)

set_target_properties(MacDockShell PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(MacDockShell
    PRIVATE Qt6::Quick
    PRIVATE Qt6::Network
    PRIVATE psapi
    PRIVATE shell32
    PRIVATE ole32
    PRIVATE dwmapi
)

include(GNUInstallDirs)
install(TARGETS MacDockShell
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

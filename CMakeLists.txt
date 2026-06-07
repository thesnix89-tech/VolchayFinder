cmake_minimum_required(VERSION 3.16)
project(MacDockShell VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)

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
)

qt_add_resources(MacDockShell macdockshell_resources
    PREFIX "/"
    FILES
        src/MacDockShell/qml/TopBarWindow.qml
        src/MacDockShell/qml/DockWindow.qml
        src/MacDockShell/qml/ControlWindow.qml
        src/MacDockShell/qml/apple_logo.svg
        src/MacDockShell/qml/apple_logo_white.svg
)

set_target_properties(MacDockShell PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(MacDockShell
    PRIVATE Qt6::Quick
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

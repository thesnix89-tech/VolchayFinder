import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Window {
    id: topBarWindow
    width: Screen.width
    height: 34
    x: 0
    y: 0
    visible: taskbarController.shellActive && taskbarController.showTopBar
    color: "transparent"
    title: "MacDockShellTopBar"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

    readonly property bool darkTheme: taskbarController.darkTheme
    readonly property color menuTextColor: darkTheme ? "#FFFFFF" : "#1B1F27"
    readonly property color menuAccentTextColor: darkTheme ? "#FFFFFF" : "#1B1F27"
    readonly property int themeAnimMs: 320

    // Frameless tool windows on Windows often skip hover until the surface is "woken up".
    MouseArea {
        anchors.fill: parent
        z: -100
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }

    Rectangle {
        anchors.fill: parent
        color: topBarWindow.darkTheme ? "#2C2C2E" : "#F5F5F7"
        border.width: 1
        border.color: topBarWindow.darkTheme ? "#3A3A3C" : "#E2E4E9"

        Behavior on color {
            ColorAnimation { duration: topBarWindow.themeAnimMs; easing.type: Easing.InOutCubic }
        }
        Behavior on border.color {
            ColorAnimation { duration: topBarWindow.themeAnimMs; easing.type: Easing.InOutCubic }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 17
            anchors.rightMargin: 14
            spacing: 18

            RowLayout {
                spacing: 9

                Item {
                    id: appleIconSlot
                    Layout.rightMargin: 7
                    readonly property int raster: Math.max(1, Math.round(18 * Screen.devicePixelRatio))
                    width: 18
                    height: 18
                    Layout.alignment: Qt.AlignVCenter

                    Image {
                        id: appleLogo
                        anchors.fill: parent
                        source: topBarWindow.darkTheme
                                ? "qrc:/src/MacDockShell/qml/apple_logo_white.svg"
                                : "qrc:/src/MacDockShell/qml/apple_logo.svg"
                        sourceSize: Qt.size(appleIconSlot.raster, appleIconSlot.raster)
                        fillMode: Image.PreserveAspectFit
                        mipmap: false
                        smooth: false
                        antialiasing: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: appleMenu.popup(appleLogo, 0, appleLogo.height + 8)
                    }
                }

                Text {
                    id: menuAppNameText
                    text: taskbarController.menuBarAppName
                    color: topBarWindow.menuAccentTextColor
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    Layout.alignment: Qt.AlignVCenter
                    Layout.rightMargin: 3

                    Behavior on color {
                        ColorAnimation { duration: topBarWindow.themeAnimMs; easing.type: Easing.InOutCubic }
                    }

                    SequentialAnimation {
                        id: menuAppNameSwap
                        NumberAnimation {
                            target: menuAppNameText
                            property: "opacity"
                            to: 0.25
                            duration: 110
                            easing.type: Easing.InCubic
                        }
                        NumberAnimation {
                            target: menuAppNameText
                            property: "opacity"
                            to: 1.0
                            duration: 220
                            easing.type: Easing.OutCubic
                        }
                    }

                    Connections {
                        target: taskbarController
                        function onMenuBarAppNameChanged() { menuAppNameSwap.restart() }
                    }
                }

                Repeater {
                    model: taskbarController.menuBarItems
                    delegate: Text {
                        text: modelData
                        color: topBarWindow.menuTextColor
                        font.pixelSize: 13
                        Layout.alignment: Qt.AlignVCenter
                        Behavior on color {
                            ColorAnimation { duration: topBarWindow.themeAnimMs; easing.type: Easing.InOutCubic }
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            StatusArea {
                darkTheme: topBarWindow.darkTheme
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }

    // Separate popup window — default Menu rendered inline in the menu bar (broken white strip).
    Menu {
        id: appleMenu
        popupType: Popup.Window
        modal: false
        padding: 6
        implicitWidth: 220
        readonly property color panelBg: topBarWindow.darkTheme ? "#323234" : "#F5F5F7"
        readonly property color panelBorder: topBarWindow.darkTheme ? "#48484A" : "#C2C2C2"
        readonly property color itemText: topBarWindow.darkTheme ? "#F2F2F7" : "#1D1D1F"
        readonly property color separatorColor: topBarWindow.darkTheme ? "#48484A" : "#D6D6D6"

        background: Rectangle {
            implicitWidth: 220
            color: appleMenu.panelBg
            radius: 8
            border.width: 1
            border.color: appleMenu.panelBorder
        }

        delegate: MenuItem {
            id: appleMenuEntry
            implicitWidth: 208
            implicitHeight: 28
            padding: 0
            arrow: Item {}
            indicator: Item {}

            contentItem: Text {
                leftPadding: 14
                verticalAlignment: Text.AlignVCenter
                text: appleMenuEntry.text
                font.pixelSize: 13
                color: appleMenuEntry.highlighted ? "#FFFFFF" : appleMenu.itemText
            }

            background: Rectangle {
                anchors.fill: parent
                anchors.leftMargin: 4
                anchors.rightMargin: 4
                radius: 5
                color: appleMenuEntry.highlighted ? "#3478F6" : "transparent"
            }
        }

        Action {
            text: "Системные настройки..."
            onTriggered: taskbarController.settingsVisible = true
        }

        MenuSeparator {
            topPadding: 4
            bottomPadding: 4
            contentItem: Rectangle {
                implicitHeight: 1
                color: appleMenu.separatorColor
            }
        }

        Action {
            text: "Выход"
            onTriggered: taskbarController.quitApplication()
        }
    }
}

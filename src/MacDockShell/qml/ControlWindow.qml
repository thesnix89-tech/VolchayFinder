import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: settingsWindow
    width: 520
    height: 380
    x: Math.round((Screen.width - width) / 2)
    y: Math.round((Screen.height - height) / 2)
    visible: taskbarController.settingsVisible
    color: "transparent"
    title: "Finder Preferences"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

    Rectangle {
        id: bgContainer
        anchors.fill: parent
        radius: 14
        color: "#F6F6F6"
        border.width: 1
        border.color: "#C5C5C5"
        clip: true

        // Sidebar Pane
        Rectangle {
            id: sidebar
            width: 160
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            color: "#EAEAEA"

            // Border separating sidebar and content
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: "#D5D5D5"
            }

            // Window controls (Traffic lights) in top left
            Row {
                x: 14
                y: 16
                spacing: 8

                // Red Close Button
                Rectangle {
                    width: 12
                    height: 12
                    radius: 6
                    color: redArea.containsMouse ? "#FF5F56" : "#FF5F56"
                    border.width: 0.5
                    border.color: "#E0443E"

                    Text {
                        anchors.centerIn: parent
                        text: "×"
                        color: "#4C0000"
                        font.pixelSize: 10
                        visible: redArea.containsMouse
                    }

                    MouseArea {
                        id: redArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (taskbarController.shellActive) {
                                taskbarController.settingsVisible = false;
                            } else {
                                taskbarController.quitApplication();
                            }
                        }
                    }
                }

                // Yellow Minimize Button (subtle decoration)
                Rectangle {
                    width: 12
                    height: 12
                    radius: 6
                    color: "#FFBD2E"
                    border.width: 0.5
                    border.color: "#DEA123"
                }

                // Green Maximize Button (subtle decoration)
                Rectangle {
                    width: 12
                    height: 12
                    radius: 6
                    color: "#27C93F"
                    border.width: 0.5
                    border.color: "#1AAB2F"
                }
            }

            // Sidebar Menu
            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 48
                spacing: 4

                // Preferences Title
                Text {
                    text: "Настройки"
                    color: "#5C5C5C"
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottomMargin: 10
                }

                // Sidebar Item: Desktop & Dock
                Rectangle {
                    width: parent.width - 16
                    height: 30
                    radius: 6
                    color: "#007AFF" // Active item in macOS blue accent
                    anchors.horizontalCenter: parent.horizontalCenter

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        spacing: 8

                        Text {
                            text: "💻"
                            font.pixelSize: 14
                        }

                        Text {
                            text: "Рабочий стол и Док"
                            color: "white"
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }
                    }
                }
            }
        }

        // Content Area (Right Side)
        Item {
            anchors.left: sidebar.right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: 20

            // Header Title
            Text {
                id: mainTitle
                text: "Рабочий стол и Док"
                color: "#1D1D1F"
                font.pixelSize: 17
                font.weight: Font.Bold
                anchors.top: parent.top
                anchors.left: parent.left
            }

            // Setting Rounded Box Group (macOS style grouped items)
            Rectangle {
                id: settingsGroup
                anchors.top: mainTitle.bottom
                anchors.topMargin: 16
                anchors.left: parent.left
                anchors.right: parent.right
                height: 180
                radius: 10
                color: "#FFFFFF"
                border.width: 1
                border.color: "#E5E5E5"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 0

                    // Option 1: Hide Taskbar
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45

                        ColumnLayout {
                            spacing: 2
                            Text {
                                text: "Автоматически скрывать панель задач Windows"
                                color: "#1D1D1F"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                            }
                            Text {
                                text: "Скрывает стандартную панель задач для лучшего вида"
                                color: "#86868B"
                                font.pixelSize: 10
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // Custom Toggle Switch
                        Item {
                            id: hideTaskbarToggle
                            width: 36
                            height: 20
                            property bool checked: true

                            Rectangle {
                                anchors.fill: parent
                                radius: 10
                                color: hideTaskbarToggle.checked ? "#34C759" : "#E9E9EA"
                                border.width: 1
                                border.color: hideTaskbarToggle.checked ? "#34C759" : "#D1D1D6"

                                Behavior on color { ColorAnimation { duration: 150 } }

                                Rectangle {
                                    width: 18
                                    height: 18
                                    radius: 9
                                    color: "white"
                                    x: hideTaskbarToggle.checked ? 17 : 1
                                    y: 1

                                    Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: hideTaskbarToggle.checked = !hideTaskbarToggle.checked
                            }
                        }
                    }

                    // Separator 1
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#F0F0F0"
                    }

                    // Option 2: Show Top Bar
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45

                        ColumnLayout {
                            spacing: 2
                            Text {
                                text: "Показывать строку меню macOS"
                                color: "#1D1D1F"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                            }
                            Text {
                                text: "Отображает статус-бар в верхней части экрана"
                                color: "#86868B"
                                font.pixelSize: 10
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // Custom Toggle Switch
                        Item {
                            id: showTopBarToggle
                            width: 36
                            height: 20
                            property bool checked: taskbarController.showTopBar

                            Rectangle {
                                anchors.fill: parent
                                radius: 10
                                color: showTopBarToggle.checked ? "#34C759" : "#E9E9EA"
                                border.width: 1
                                border.color: showTopBarToggle.checked ? "#34C759" : "#D1D1D6"

                                Behavior on color { ColorAnimation { duration: 150 } }

                                Rectangle {
                                    width: 18
                                    height: 18
                                    radius: 9
                                    color: "white"
                                    x: showTopBarToggle.checked ? 17 : 1
                                    y: 1

                                    Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: showTopBarToggle.checked = !showTopBarToggle.checked
                            }
                        }
                    }

                    // Separator 2
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#F0F0F0"
                    }

                    // Option 3: Icon Size Slider
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45

                        ColumnLayout {
                            spacing: 2
                            Text {
                                text: "Размер иконок дока"
                                color: "#1D1D1F"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                            }
                            Text {
                                text: "Выбор размера значков панели (в пикселях)"
                                color: "#86868B"
                                font.pixelSize: 10
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // Custom Slider
                        Slider {
                            id: iconSizeSlider
                            from: 36
                            to: 64
                            value: taskbarController.dockIconSize
                            stepSize: 1

                            background: Rectangle {
                                x: iconSizeSlider.leftPadding
                                y: iconSizeSlider.topPadding + iconSizeSlider.availableHeight / 2 - height / 2
                                implicitWidth: 100
                                implicitHeight: 4
                                width: iconSizeSlider.availableWidth
                                height: implicitHeight
                                radius: 2
                                color: "#E5E5EA"

                                Rectangle {
                                    width: iconSizeSlider.visualPosition * parent.width
                                    height: parent.height
                                    color: "#007AFF"
                                    radius: 2
                                }
                            }

                            handle: Rectangle {
                                x: iconSizeSlider.leftPadding + iconSizeSlider.visualPosition * (iconSizeSlider.availableWidth - width)
                                y: iconSizeSlider.topPadding + iconSizeSlider.availableHeight / 2 - height / 2
                                implicitWidth: 16
                                implicitHeight: 16
                                radius: 8
                                color: "white"
                                border.color: "#C5C5C5"
                                border.width: 0.5
                            }
                        }

                        // Text indicator showing current value
                        Text {
                            text: Math.round(iconSizeSlider.value) + " px"
                            color: "#1D1D1F"
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            Layout.preferredWidth: 36
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }

            // Bottom Buttons (Apply and Exit)
            RowLayout {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 12

                // Red/Grey Button to Exit App
                Button {
                    id: exitBtn
                    contentItem: Text {
                        text: "Выход"
                        color: exitBtnMouse.containsMouse ? "#B3261E" : "#8E8E93"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        implicitWidth: 80
                        implicitHeight: 28
                        color: "transparent"
                    }

                    MouseArea {
                        id: exitBtnMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: taskbarController.quitApplication()
                    }
                }

                Item { Layout.fillWidth: true }

                // macOS Accent Blue Apply Button
                Button {
                    id: applyBtn
                    contentItem: Text {
                        text: "Применить"
                        color: "white"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        implicitWidth: 100
                        implicitHeight: 28
                        radius: 6
                        color: applyBtnMouse.containsMouse ? "#0062CC" : "#007AFF"

                        Behavior on color { ColorAnimation { duration: 100 } }
                    }

                    MouseArea {
                        id: applyBtnMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            // If hideTaskbarToggle is not checked, we can show standard taskbar
                            if (!hideTaskbarToggle.checked) {
                                taskbarController.showTaskbar();
                            }
                            taskbarController.apply(showTopBarToggle.checked, Math.round(iconSizeSlider.value));
                        }
                    }
                }
            }
        }
    }
}

import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Window {
    id: settingsWindow
    width: 680
    height: 520

    property int settingsPage: 0
    property string explorerIconStyle: taskbarController.explorerIconStyle
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
        clip: true

        // Sidebar Pane
        Rectangle {
            id: sidebar
            width: 180
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            color: "#EAEAEA"
            radius: 14 // Round the left corners to match the window container

            // Overlap helper rectangle to keep the dividing border edge sharp
            Rectangle {
                width: 20
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                color: "#EAEAEA"
            }

            // Drag area to move the frameless window
            MouseArea {
                anchors.fill: parent
                property point clickPos: "0,0"
                onPressed: function(mouse) {
                    clickPos = Qt.point(mouse.x, mouse.y)
                }
                onPositionChanged: function(mouse) {
                    var delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y)
                    settingsWindow.x += delta.x
                    settingsWindow.y += delta.y
                }
            }

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
                x: 16
                y: 18
                spacing: 8
                z: 10 // Ensure it sits on top of the drag MouseArea

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
                        cursorShape: Qt.PointingHandCursor
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
            ColumnLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 44
                spacing: 8
                z: 10 // Ensure it sits on top of the drag MouseArea

                // Preferences Title
                Text {
                    text: "Настройки"
                    color: "#5C5C5C"
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    Layout.leftMargin: 16
                    Layout.topMargin: 6
                }

                // Sidebar Item: Desktop & Dock
                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 8
                    Layout.rightMargin: 8
                    height: 30
                    radius: 6
                    color: settingsPage === 0 ? "#007AFF" : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        spacing: 8

                        Text {
                            text: "💻"
                            font.pixelSize: 13
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Text {
                            text: "Рабочий стол и Док"
                            color: settingsPage === 0 ? "white" : "#1D1D1F"
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settingsPage = 0
                    }
                }

                // Sidebar Item: Explorer
                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 8
                    Layout.rightMargin: 8
                    height: 30
                    radius: 6
                    color: settingsPage === 1 ? "#007AFF" : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        spacing: 8

                        Text {
                            text: "📁"
                            font.pixelSize: 13
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Text {
                            text: "Проводник"
                            color: settingsPage === 1 ? "white" : "#1D1D1F"
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settingsPage = 1
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
            anchors.bottomMargin: 16

            // Header Drag Area to move window from the top margin of content pane
            MouseArea {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 40
                property point clickPos: "0,0"
                onPressed: function(mouse) {
                    clickPos = Qt.point(mouse.x, mouse.y)
                }
                onPositionChanged: function(mouse) {
                    var delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y)
                    settingsWindow.x += delta.x
                    settingsWindow.y += delta.y
                }
            }

            // Header Title
            Text {
                id: mainTitle
                text: settingsPage === 0 ? "Рабочий стол и Док" : "Проводник"
                color: "#1D1D1F"
                font.pixelSize: 17
                font.weight: Font.Bold
                anchors.top: parent.top
                anchors.topMargin: 10
                anchors.left: parent.left
            }

            component ExplorerIconCard : Rectangle {
                id: card
                signal clicked()

                property alias previewSource: previewImage.source
                property string title
                property string subtitle
                property bool selected

                width: 148
                height: 164
                radius: 10
                clip: true
                color: "#FAFAFA"
                border.width: selected ? 2 : 1
                border.color: selected ? "#007AFF" : "#E5E5E5"

                Column {
                    anchors.centerIn: parent
                    spacing: 8

                    Item {
                        width: 64
                        height: 64
                        anchors.horizontalCenter: parent.horizontalCenter

                        Image {
                            id: previewImage
                            anchors.fill: parent
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: card.title
                        color: "#1D1D1F"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: card.subtitle
                        color: "#86868B"
                        font.pixelSize: 10
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: card.clicked()
                }
            }

            ScrollView {
                id: settingsScroll
                anchors.top: mainTitle.bottom
                anchors.topMargin: 16
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 52
                clip: true

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                Column {
                    width: settingsScroll.availableWidth
                    spacing: 0

            // Setting Rounded Box Group (macOS style grouped items)
            Rectangle {
                id: settingsGroup
                visible: settingsPage === 0
                height: settingsPage === 0 ? innerLayout.implicitHeight + 24 : 0
                width: parent.width
                radius: 10
                color: "#FFFFFF"
                border.width: 1
                border.color: "#E5E5E5"

                ColumnLayout {
                    id: innerLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 0

                    // Option 1: Hide Taskbar
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45

                        ColumnLayout {
                            spacing: 2
                            Layout.alignment: Qt.AlignVCenter
                            Layout.fillWidth: true
                            Text {
                                text: "Автоматически скрывать панель задач Windows"
                                color: "#1D1D1F"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: "Скрывает стандартную панель задач для лучшего вида"
                                color: "#86868B"
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacToggle {
                            id: hideTaskbarToggle
                            checked: taskbarController.autoHideWindowsTaskbar
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: hideTaskbarToggle.checked = !hideTaskbarToggle.checked
                        }
                    }

                    // Separator 1a
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#F0F0F0"
                    }

                    // Option 1b: Keep Windows auto-hide after exit
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45

                        ColumnLayout {
                            spacing: 2
                            Layout.alignment: Qt.AlignVCenter
                            Layout.fillWidth: true
                            Text {
                                text: "Оставить панель задач Windows скрытой после выхода"
                                color: "#1D1D1F"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: "При выходе сохраняет включённой настройку «Автоматически скрывать панель задач» в Windows"
                                color: "#86868B"
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacToggle {
                            id: keepTaskbarAutoHideOnExitToggle
                            checked: taskbarController.keepTaskbarAutoHideOnExit
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: keepTaskbarAutoHideOnExitToggle.checked = !keepTaskbarAutoHideOnExitToggle.checked
                        }
                    }

                    // Separator 1b
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#F0F0F0"
                    }

                    // Option 1c: Sync dock with Windows taskbar pins on startup
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45

                        ColumnLayout {
                            spacing: 2
                            Layout.alignment: Qt.AlignVCenter
                            Layout.fillWidth: true
                            Text {
                                text: "При запуске прикреплять приложения с панели задач Windows"
                                color: "#1D1D1F"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: "Сканирует закреплённые на панели задач Windows приложения и добавляет их в док"
                                color: "#86868B"
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacToggle {
                            id: syncTaskbarPinsToggle
                            checked: taskbarController.syncDockWithWindowsTaskbarOnStartup
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: syncTaskbarPinsToggle.checked = !syncTaskbarPinsToggle.checked
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
                            Layout.alignment: Qt.AlignVCenter
                            Layout.fillWidth: true
                            Text {
                                text: "Показывать строку меню macOS"
                                color: "#1D1D1F"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: "Отображает статус-бар в верхней части экрана"
                                color: "#86868B"
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacToggle {
                            id: showTopBarToggle
                            checked: taskbarController.showTopBar
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: showTopBarToggle.checked = !showTopBarToggle.checked
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
                            Layout.alignment: Qt.AlignVCenter
                            Layout.fillWidth: true
                            Text {
                                text: "Размер иконок дока"
                                color: "#1D1D1F"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: "Выбор размера значков панели (в пикселях)"
                                color: "#86868B"
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        // Custom Slider
                        Slider {
                            id: iconSizeSlider
                            from: 36
                            to: 64
                            value: taskbarController.dockIconSize
                            stepSize: 1
                            Layout.preferredWidth: 100
                            Layout.preferredHeight: 20
                            Layout.alignment: Qt.AlignVCenter

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
                            Layout.alignment: Qt.AlignVCenter
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    // Separator 3
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#F0F0F0"
                    }

                    // Option 4: Dock Hover Bounce
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45

                        ColumnLayout {
                            spacing: 2
                            Layout.alignment: Qt.AlignVCenter
                            Layout.fillWidth: true
                            opacity: staticIconsToggle.checked ? 0.4 : 1.0
                            Behavior on opacity { NumberAnimation { duration: 150 } }
                            Text {
                                text: "Подпрыгивание иконок при наведении"
                                color: "#1D1D1F"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: "Иконки приподнимаются, когда вы наводите на них курсор"
                                color: "#86868B"
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacToggle {
                            id: hoverBounceToggle
                            checked: taskbarController.dockHoverBounce
                            enabled: !staticIconsToggle.checked
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: hoverBounceToggle.checked = !hoverBounceToggle.checked
                        }
                    }

                    // Separator 4
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#F0F0F0"
                    }

                    // Option 4b: Drag fade
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45

                        ColumnLayout {
                            spacing: 2
                            Layout.alignment: Qt.AlignVCenter
                            Layout.fillWidth: true
                            Text {
                                text: "Делать иконки прозрачными во время перетаскивания"
                                color: "#1D1D1F"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: "Через секунду удержания иконка становится полупрозрачной, как на macOS"
                                color: "#86868B"
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacToggle {
                            id: dragFadeToggle
                            checked: taskbarController.dockDragFadeEnabled
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: dragFadeToggle.checked = !dragFadeToggle.checked
                        }
                    }

                    // Separator 4b
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#F0F0F0"
                    }

                    // Option 5: Dark Theme
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45

                        ColumnLayout {
                            spacing: 2
                            Layout.alignment: Qt.AlignVCenter
                            Layout.fillWidth: true
                            Text {
                                text: "Тёмная тема"
                                color: "#1D1D1F"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: "Тёмный док и верхняя панель в стиле macOS Monterey"
                                color: "#86868B"
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacToggle {
                            id: darkThemeToggle
                            checked: taskbarController.darkTheme
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: darkThemeToggle.checked = !darkThemeToggle.checked
                        }
                    }

                    // Separator 5
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#F0F0F0"
                    }

                    // Option 6: Static Dock Icons
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45

                        ColumnLayout {
                            spacing: 2
                            Layout.alignment: Qt.AlignVCenter
                            Layout.fillWidth: true
                            Text {
                                text: "Статичные иконки дока"
                                color: "#1D1D1F"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: "Иконки не двигаются и не увеличиваются при наведении"
                                color: "#86868B"
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacToggle {
                            id: staticIconsToggle
                            checked: taskbarController.dockStaticIcons
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: staticIconsToggle.checked = !staticIconsToggle.checked
                        }
                    }

                    // Separator 6
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#F0F0F0"
                    }

                    // Option 7: Start with Windows
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45

                        ColumnLayout {
                            spacing: 2
                            Layout.alignment: Qt.AlignVCenter
                            Layout.fillWidth: true
                            Text {
                                text: "Запускать с Windows"
                                color: "#1D1D1F"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: "Автоматически запускать оболочку при входе в систему"
                                color: "#86868B"
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacToggle {
                            id: startWithWindowsToggle
                            checked: taskbarController.startWithWindows
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: startWithWindowsToggle.checked = !startWithWindowsToggle.checked
                        }
                    }
                }
            }

            Rectangle {
                id: explorerSettingsGroup
                visible: settingsPage === 1
                width: parent.width
                height: settingsPage === 1 ? explorerInner.implicitHeight + 24 : 0
                radius: 10
                color: "#FFFFFF"
                border.width: 1
                border.color: "#E5E5E5"
                clip: true

                Column {
                    id: explorerInner
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 14

                    Text {
                        text: "Иконка проводника"
                        color: "#1D1D1F"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }

                    Text {
                        width: parent.width
                        text: "Как отображать File Explorer в доке"
                        color: "#86868B"
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }

                    Row {
                        spacing: 16

                        ExplorerIconCard {
                            selected: settingsWindow.explorerIconStyle === "default"
                            previewSource: dockModel.explorerDefaultIconUrl()
                            title: "Стандартная"
                            subtitle: "Windows"
                            onClicked: settingsWindow.explorerIconStyle = "default"
                        }

                        ExplorerIconCard {
                            selected: settingsWindow.explorerIconStyle === "macos"
                            previewSource: dockModel.explorerMacIconUrl()
                            title: "macOS Finder"
                            subtitle: "Как на Mac"
                            onClicked: settingsWindow.explorerIconStyle = "macos"
                        }
                    }
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
                    text: "Выход"
                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 30

                    contentItem: Text {
                        text: exitBtn.text
                        color: exitBtn.hovered ? "#FF453A" : "#8E8E93"
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: "transparent"
                    }

                    onClicked: taskbarController.quitApplication()
                }

                Item { Layout.fillWidth: true }

                // macOS Accent Blue Apply Button
                Button {
                    id: applyBtn
                    text: "Применить"
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 30

                    contentItem: Text {
                        text: applyBtn.text
                        color: "white"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 6
                        color: applyBtn.down ? "#0051C7" : (applyBtn.hovered ? "#0062E3" : "#007AFF")

                        Behavior on color { ColorAnimation { duration: 100 } }
                    }

                    onClicked: {
                        taskbarController.apply(hideTaskbarToggle.checked, keepTaskbarAutoHideOnExitToggle.checked, syncTaskbarPinsToggle.checked, showTopBarToggle.checked, Math.round(iconSizeSlider.value), hoverBounceToggle.checked, dragFadeToggle.checked, staticIconsToggle.checked, darkThemeToggle.checked, startWithWindowsToggle.checked, settingsWindow.explorerIconStyle);
                        if (syncTaskbarPinsToggle.checked)
                            dockModel.syncFromWindowsTaskbarPins()
                        dockModel.refresh()
                    }
                }
            }
        }
        // Window Border on top of everything
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.width: 1
            border.color: "#C5C5C5"
            radius: 14
            z: 100
        }
    }
}

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
    property string trashIconStyle: taskbarController.trashIconStyle
    property string menuBarIconStyle: taskbarController.menuBarIconStyle
    property string dockLightStyle: taskbarController.dockLightStyle
    property bool pinFromTaskbarRequested: false

    readonly property bool darkTheme: taskbarController.darkTheme
    readonly property color windowBg: darkTheme ? "#1C1C1E" : "#F6F6F6"
    readonly property color sidebarBg: darkTheme ? "#2C2C2E" : "#EAEAEA"
    readonly property color cardBg: darkTheme ? "#2C2C2E" : "#FFFFFF"
    readonly property color groupDivider: darkTheme ? "#38383A" : "#F0F0F0"
    readonly property color primaryText: darkTheme ? "#FFFFFF" : "#1D1D1F"
    readonly property color secondaryText: darkTheme ? "#8E8E93" : "#86868B"
    readonly property color separatorColor: darkTheme ? "#38383A" : "#D5D5D5"
    readonly property color borderColor: darkTheme ? "#48484A" : "#E5E5E5"
    readonly property color sidebarTitleColor: darkTheme ? "#8E8E93" : "#5C5C5C"
    readonly property color navInactiveText: darkTheme ? "#EBEBF5" : "#1D1D1F"
    readonly property color pinButtonBg: darkTheme ? "#3A3A3C" : "#FFFFFF"
    readonly property color pinButtonBorder: darkTheme ? "#48484A" : "#D1D1D6"
    readonly property color subtleButtonBg: darkTheme ? "#3A3A3C" : "#FFFFFF"
    readonly property color subtleButtonBorder: darkTheme ? "#48484A" : "#D1D1D6"
    readonly property color sliderTrack: darkTheme ? "#48484A" : "#E5E5EA"
    readonly property color sliderHandleBorder: darkTheme ? "#636366" : "#C5C5C5"
    readonly property color sectionSeparator: darkTheme ? "#38383A" : "#ECECEC"
    readonly property color outerBorder: darkTheme ? "#48484A" : "#C5C5C5"
    readonly property color buttonBg: darkTheme ? "#48484A" : "#FFFFFF"
    readonly property color buttonBorder: darkTheme ? "#636366" : "#D1D1D6"
    readonly property color buttonText: primaryText
    readonly property var languageCodes: ["system", "en", "uk", "ru"]
    readonly property color scrollTrackBg: darkTheme ? "#3A3A3C" : "#E5E5EA"
    readonly property int settingsScrollGutter: settingsVScroll.trackWidth
    readonly property int settingsCardInset: 8
    readonly property int settingsScrollInsetLeft: settingsCardInset
    readonly property int settingsScrollInsetRight: settingsCardInset

    function syncLanguageIndex() {
        const idx = languageCodes.indexOf(taskbarController.uiLanguage)
        languagePopup.currentIndex = idx >= 0 ? idx : 0
    }

    onVisibleChanged: {
        if (!visible) {
            pinFromTaskbarRequested = false
            return
        }
        explorerIconStyle = taskbarController.explorerIconStyle
        trashIconStyle = taskbarController.trashIconStyle
        menuBarIconStyle = taskbarController.menuBarIconStyle
        dockLightStyle = taskbarController.dockLightStyle
        syncLanguageIndex()
    }

    Connections {
        target: taskbarController
        function onLanguageChanged() {
            settingsWindow.syncLanguageIndex()
        }
    }

    x: Math.round((Screen.width - width) / 2)
    y: Math.round((Screen.height - height) / 2)
    visible: taskbarController.settingsVisible
    color: "transparent"
    title: qsTr("Finder Preferences")
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

    Rectangle {
        id: bgContainer
        anchors.fill: parent
        radius: 14
        color: settingsWindow.windowBg
        clip: true

        Behavior on color {
            ColorAnimation { duration: 180; easing.type: Easing.OutCubic }
        }

        // Sidebar Pane
        Rectangle {
            id: sidebar
            width: 180
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            color: settingsWindow.sidebarBg
            radius: 14 // Round the left corners to match the window container
            clip: true

            Behavior on color {
                ColorAnimation { duration: 180; easing.type: Easing.OutCubic }
            }

            // Overlap helper rectangle to keep the dividing border edge sharp
            Rectangle {
                width: 20
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                color: settingsWindow.sidebarBg
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
                color: settingsWindow.separatorColor
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
                    text: qsTr("Settings")
                    color: settingsWindow.sidebarTitleColor
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
                            text: qsTr("Desktop & Dock")
                            color: settingsPage === 0 ? "white" : settingsWindow.navInactiveText
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
                            text: qsTr("Explorer")
                            color: settingsPage === 1 ? "white" : settingsWindow.navInactiveText
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

            Rectangle {
                anchors.fill: parent
                color: settingsWindow.windowBg

                Behavior on color {
                    ColorAnimation { duration: 180; easing.type: Easing.OutCubic }
                }
            }

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
                text: settingsPage === 0 ? qsTr("Desktop & Dock") : qsTr("Explorer")
                color: settingsWindow.primaryText
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
                property bool darkPreview: false
                property bool colorPreview: false
                property color previewFill: "#F3F3F3"

                width: 148
                height: 164
                radius: 10
                clip: true
                color: darkPreview ? "#2C2C2E" : "#FAFAFA"
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
                            visible: !card.colorPreview
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: 14
                            color: card.previewFill
                            border.width: 1
                            border.color: "#D1D1D6"
                            visible: card.colorPreview
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "+"
                            color: settingsWindow.secondaryText
                            font.pixelSize: 28
                            font.weight: Font.Medium
                            visible: !card.colorPreview
                                    && (previewImage.source.toString().length === 0
                                    || previewImage.status !== Image.Ready)
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: card.title
                        color: card.darkPreview ? "#F5F5F7" : "#1D1D1F"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: card.subtitle
                        color: card.darkPreview ? "#B7BDC9" : "#86868B"
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
                leftPadding: settingsWindow.settingsScrollGutter
                clip: true

                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                background: Rectangle {
                    color: "transparent"
                }

                Component.onCompleted: {
                    if (contentItem) {
                        contentItem.boundsBehavior = Flickable.StopAtBounds
                        contentItem.clip = true
                    }
                }

                ScrollBar.vertical: MacScrollBar {
                    id: settingsVScroll
                    parent: settingsScroll
                    anchors.left: settingsScroll.left
                    anchors.top: settingsScroll.top
                    anchors.bottom: settingsScroll.bottom
                    darkTheme: settingsWindow.darkTheme
                    trackColor: settingsWindow.scrollTrackBg
                    alwaysVisible: true
                }

                Rectangle {
                    width: settingsScroll.availableWidth
                    implicitHeight: scrollColumn.implicitHeight
                    color: settingsWindow.windowBg

                    Behavior on color {
                        ColorAnimation { duration: 180; easing.type: Easing.OutCubic }
                    }

                    Column {
                        id: scrollColumn
                        width: parent.width
                        spacing: 0

            // Setting Rounded Box Group (macOS style grouped items)
            Item {
                width: scrollColumn.width
                height: settingsPage === 0 ? settingsGroup.height : 0
                visible: settingsPage === 0

                Rectangle {
                    id: settingsGroup
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: settingsWindow.settingsScrollInsetLeft
                    anchors.rightMargin: settingsWindow.settingsScrollInsetRight
                    height: innerLayout.implicitHeight + 24
                    radius: 10
                    clip: true
                    color: settingsWindow.cardBg
                    border.width: 1
                    border.color: settingsWindow.borderColor

                    Behavior on color {
                        ColorAnimation { duration: 180; easing.type: Easing.OutCubic }
                    }

                    ColumnLayout {
                        id: innerLayout
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 12
                        spacing: 0

                        // Option 0: Interface language
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45

                        ColumnLayout {
                            spacing: 2
                            Layout.alignment: Qt.AlignVCenter
                            Layout.fillWidth: true
                            Text {
                                text: qsTr("Interface language")
                                color: settingsWindow.primaryText
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: qsTr("Choose the language for menus and settings")
                                color: settingsWindow.secondaryText
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacPopupButton {
                            id: languagePopup
                            Layout.preferredWidth: 180
                            Layout.alignment: Qt.AlignVCenter
                            model: settingsWindow.languageCodes
                            darkTheme: taskbarController.darkTheme
                            textForIndex: function(index) {
                                return taskbarController.languageDisplayName(settingsWindow.languageCodes[index])
                            }
                            onValueActivated: function(code) {
                                taskbarController.setUiLanguage(code)
                            }
                            Component.onCompleted: settingsWindow.syncLanguageIndex()
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: settingsWindow.groupDivider
                    }

                    // Option 1: Hide Taskbar
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45

                        ColumnLayout {
                            spacing: 2
                            Layout.alignment: Qt.AlignVCenter
                            Layout.fillWidth: true
                            Text {
                                text: qsTr("Automatically hide the Windows taskbar")
                                color: settingsWindow.primaryText
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: qsTr("Hides the standard taskbar for a cleaner look")
                                color: settingsWindow.secondaryText
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacToggle {
                            id: hideTaskbarToggle
                            darkTheme: settingsWindow.darkTheme
                            checked: taskbarController.autoHideWindowsTaskbar
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: hideTaskbarToggle.checked = !hideTaskbarToggle.checked
                        }
                    }

                    // Separator 1a
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: settingsWindow.groupDivider
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
                                text: qsTr("Keep the Windows taskbar hidden after exit")
                                color: settingsWindow.primaryText
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: qsTr("On exit, keeps Windows \"Automatically hide the taskbar\" enabled")
                                color: settingsWindow.secondaryText
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacToggle {
                            id: keepTaskbarAutoHideOnExitToggle
                            darkTheme: settingsWindow.darkTheme
                            checked: taskbarController.keepTaskbarAutoHideOnExit
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: keepTaskbarAutoHideOnExitToggle.checked = !keepTaskbarAutoHideOnExitToggle.checked
                        }
                    }

                    // Separator 1b
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: settingsWindow.groupDivider
                    }

                    // Option 1c: Pin apps from Windows taskbar (manual, on Apply)
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: qsTr("Pin apps from the Windows taskbar")
                            color: settingsWindow.primaryText
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            text: qsTr("One-time action: click the button, then Apply. Repeat on the next launch or when reopening settings if you want to sync the dock with the taskbar again")
                            color: settingsWindow.secondaryText
                            font.pixelSize: 10
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }

                        Rectangle {
                            id: pinFromTaskbarRow
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            radius: 6
                            color: pinFromTaskbarMouse.pressed ? settingsWindow.pinButtonBorder
                                    : (pinFromTaskbarMouse.containsMouse
                                       ? (settingsWindow.darkTheme ? "#48484A" : "#ECECEF")
                                       : settingsWindow.pinButtonBg)
                            border.width: 1
                            border.color: settingsWindow.pinFromTaskbarRequested ? "#34C759" : settingsWindow.pinButtonBorder

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 10

                                Rectangle {
                                    width: 20
                                    height: 20
                                    radius: 4
                                    color: settingsWindow.pinFromTaskbarRequested ? "#34C759" : "#FFFFFF"
                                    border.width: settingsWindow.pinFromTaskbarRequested ? 0 : 1.5
                                    border.color: settingsWindow.pinFromTaskbarRequested ? "#34C759" : "#86868B"
                                    Layout.alignment: Qt.AlignVCenter

                                    Text {
                                        anchors.centerIn: parent
                                        visible: settingsWindow.pinFromTaskbarRequested
                                        text: "✓"
                                        color: "#FFFFFF"
                                        font.pixelSize: 13
                                        font.weight: Font.Bold
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("Pin from Windows taskbar")
                                    color: settingsWindow.primaryText
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    wrapMode: Text.WordWrap
                                }
                            }

                            MouseArea {
                                id: pinFromTaskbarMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settingsWindow.pinFromTaskbarRequested = !settingsWindow.pinFromTaskbarRequested
                            }
                        }
                    }

                    // Separator 1
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: settingsWindow.groupDivider
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
                                text: qsTr("Show macOS menu bar")
                                color: settingsWindow.primaryText
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: qsTr("Displays the status bar at the top of the screen")
                                color: settingsWindow.secondaryText
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacToggle {
                            id: showTopBarToggle
                            darkTheme: settingsWindow.darkTheme
                            checked: taskbarController.showTopBar
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: showTopBarToggle.checked = !showTopBarToggle.checked
                        }
                    }

                    // Separator 2
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: settingsWindow.groupDivider
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
                                text: qsTr("Dock icon size")
                                color: settingsWindow.primaryText
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: qsTr("Choose the dock icon size in pixels")
                                color: settingsWindow.secondaryText
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
                                color: settingsWindow.sliderTrack

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
                                border.color: settingsWindow.sliderHandleBorder
                                border.width: 0.5
                            }
                        }

                        // Text indicator showing current value
                        Text {
                            text: Math.round(iconSizeSlider.value) + " px"
                            color: settingsWindow.primaryText
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
                        color: settingsWindow.groupDivider
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
                                text: qsTr("Bounce icons on hover")
                                color: settingsWindow.primaryText
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: qsTr("Icons lift when you hover over them")
                                color: settingsWindow.secondaryText
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacToggle {
                            id: hoverBounceToggle
                            darkTheme: settingsWindow.darkTheme
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
                        color: settingsWindow.groupDivider
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
                                text: qsTr("Fade icons while dragging")
                                color: settingsWindow.primaryText
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: qsTr("Outside the dock the icon is semi-transparent; inside it stays opaque, like on macOS")
                                color: settingsWindow.secondaryText
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacToggle {
                            id: dragFadeToggle
                            darkTheme: settingsWindow.darkTheme
                            checked: taskbarController.dockDragFadeEnabled
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: dragFadeToggle.checked = !dragFadeToggle.checked
                        }
                    }

                    // Separator 4b
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: settingsWindow.groupDivider
                    }

                    // Option 5: Appearance
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        ColumnLayout {
                            spacing: 2
                            Layout.fillWidth: true
                            Text {
                                text: qsTr("Appearance")
                                color: settingsWindow.primaryText
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: qsTr("Choose the look of the dock and menu bar")
                                color: settingsWindow.secondaryText
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacAppearancePicker {
                            Layout.fillWidth: true
                            Layout.preferredHeight: implicitHeight
                            Layout.topMargin: 4
                            Layout.bottomMargin: 8
                            labelColor: settingsWindow.primaryText
                            currentMode: taskbarController.appearanceMode
                            onModeSelected: function(mode) {
                                taskbarController.appearanceMode = mode
                            }
                        }
                    }

                    // Separator 5a
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: settingsWindow.groupDivider
                    }

                    // Option 5a: Light-theme dock background
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        opacity: settingsWindow.darkTheme ? 0.45 : 1.0
                        enabled: !settingsWindow.darkTheme

                        Text {
                            text: qsTr("Dock background (light theme)")
                            color: settingsWindow.primaryText
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }

                        Text {
                            text: qsTr("On dark theme the dock is always black. This choice applies only to the light theme.")
                            color: settingsWindow.secondaryText
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Row {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 164
                            spacing: 16

                            ExplorerIconCard {
                                selected: settingsWindow.dockLightStyle === "white"
                                colorPreview: true
                                previewFill: "#F3F3F3"
                                title: qsTr("White")
                                subtitle: qsTr("Light style")
                                onClicked: settingsWindow.dockLightStyle = "white"
                            }

                            ExplorerIconCard {
                                selected: settingsWindow.dockLightStyle === "macos27"
                                colorPreview: true
                                previewFill: "#5E5E5E"
                                title: qsTr("macOS 27")
                                subtitle: qsTr("Gray like on Mac")
                                onClicked: settingsWindow.dockLightStyle = "macos27"
                            }
                        }
                    }

                    // Separator 5
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: settingsWindow.groupDivider
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
                                text: qsTr("Static dock icons")
                                color: settingsWindow.primaryText
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: qsTr("Icons do not move or magnify on hover")
                                color: settingsWindow.secondaryText
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacToggle {
                            id: staticIconsToggle
                            darkTheme: settingsWindow.darkTheme
                            checked: taskbarController.dockStaticIcons
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: staticIconsToggle.checked = !staticIconsToggle.checked
                        }
                    }

                    // Separator 5b
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: settingsWindow.groupDivider
                    }

                    // Option 6b: Separate transient apps section
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45

                        ColumnLayout {
                            spacing: 2
                            Layout.alignment: Qt.AlignVCenter
                            Layout.fillWidth: true
                            Text {
                                text: qsTr("Pin new apps in a separate section")
                                color: settingsWindow.primaryText
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: qsTr("Unpinned running apps appear between pinned icons and the trash, like on macOS")
                                color: settingsWindow.secondaryText
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacToggle {
                            id: separateTransientToggle
                            darkTheme: settingsWindow.darkTheme
                            checked: taskbarController.dockSeparateTransientApps
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: separateTransientToggle.checked = !separateTransientToggle.checked
                        }
                    }

                    // Separator 6
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: settingsWindow.groupDivider
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
                                text: qsTr("Start with Windows")
                                color: settingsWindow.primaryText
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: qsTr("Automatically start the shell when you sign in")
                                color: settingsWindow.secondaryText
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacToggle {
                            id: startWithWindowsToggle
                            darkTheme: settingsWindow.darkTheme
                            checked: taskbarController.startWithWindows
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: startWithWindowsToggle.checked = !startWithWindowsToggle.checked
                        }
                    }
                }
            }
            }

            Item {
                width: scrollColumn.width
                height: settingsPage === 1 ? explorerSettingsGroup.height : 0
                visible: settingsPage === 1

                Rectangle {
                    id: explorerSettingsGroup
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: settingsWindow.settingsScrollInsetLeft
                    anchors.rightMargin: settingsWindow.settingsScrollInsetRight
                    height: explorerInner.implicitHeight + 24
                    radius: 10
                    color: settingsWindow.cardBg
                    border.width: 1
                    border.color: settingsWindow.borderColor

                    Behavior on color {
                        ColorAnimation { duration: 180; easing.type: Easing.OutCubic }
                    }
                    clip: true

                    Column {
                        id: explorerInner
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 12
                    spacing: 14

                    Text {
                        text: qsTr("Menu bar icon")
                        color: settingsWindow.primaryText
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }

                    Text {
                        width: parent.width
                        text: qsTr("Icon on the left side of the top bar")
                        color: settingsWindow.secondaryText
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }

                    Row {
                        spacing: 16

                        ExplorerIconCard {
                            selected: settingsWindow.menuBarIconStyle === "apple"
                            darkPreview: settingsWindow.darkTheme
                            previewSource: taskbarController.menuBarIconPreviewUrl("apple", settingsWindow.darkTheme)
                            title: "Apple"
                            subtitle: qsTr("Like on Mac")
                            onClicked: settingsWindow.menuBarIconStyle = "apple"
                        }

                        ExplorerIconCard {
                            selected: settingsWindow.menuBarIconStyle === "star"
                            darkPreview: settingsWindow.darkTheme
                            previewSource: taskbarController.menuBarIconPreviewUrl("star", settingsWindow.darkTheme)
                            title: "Megushell"
                            subtitle: qsTr("Star")
                            onClicked: settingsWindow.menuBarIconStyle = "star"
                        }

                        ExplorerIconCard {
                            selected: settingsWindow.menuBarIconStyle === "windows"
                            darkPreview: settingsWindow.darkTheme
                            previewSource: taskbarController.menuBarIconPreviewUrl("windows", settingsWindow.darkTheme)
                            title: "Windows"
                            subtitle: qsTr("Grid")
                            onClicked: settingsWindow.menuBarIconStyle = "windows"
                        }

                        ExplorerIconCard {
                            selected: settingsWindow.menuBarIconStyle === "custom"
                            darkPreview: settingsWindow.darkTheme
                            previewSource: taskbarController.menuBarIconPreviewUrl("custom", settingsWindow.darkTheme)
                            title: qsTr("Custom")
                            subtitle: qsTr("From file")
                            onClicked: settingsWindow.menuBarIconStyle = "custom"
                        }
                    }

                    Button {
                        id: importMenuBarIconBtn
                        text: qsTr("Import…")
                        implicitHeight: 30
                        onClicked: {
                            if (taskbarController.importCustomMenuBarIcon())
                                settingsWindow.menuBarIconStyle = "custom"
                        }

                        contentItem: Text {
                            text: importMenuBarIconBtn.text
                            color: settingsWindow.buttonText
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 6
                            color: importMenuBarIconBtn.down ? settingsWindow.sectionSeparator
                                   : (importMenuBarIconBtn.hovered
                                      ? (settingsWindow.darkTheme ? "#5A5A5E" : "#F5F5F7")
                                      : settingsWindow.buttonBg)
                            border.width: 1
                            border.color: settingsWindow.buttonBorder
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: settingsWindow.sectionSeparator
                    }

                    Text {
                        text: qsTr("Explorer icon")
                        color: settingsWindow.primaryText
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }

                    Text {
                        width: parent.width
                        text: qsTr("How to display File Explorer in the dock")
                        color: settingsWindow.secondaryText
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }

                    Row {
                        spacing: 16

                        ExplorerIconCard {
                            selected: settingsWindow.explorerIconStyle === "default"
                            previewSource: dockModel.explorerDefaultIconUrl()
                            title: qsTr("Default")
                            subtitle: "Windows"
                            onClicked: settingsWindow.explorerIconStyle = "default"
                        }

                        ExplorerIconCard {
                            selected: settingsWindow.explorerIconStyle === "macos"
                            previewSource: dockModel.explorerMacIconUrl()
                            title: "macOS Finder"
                            subtitle: qsTr("Like on Mac")
                            onClicked: settingsWindow.explorerIconStyle = "macos"
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: settingsWindow.sectionSeparator
                    }

                    Text {
                        text: qsTr("Trash icon")
                        color: settingsWindow.primaryText
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }

                    Text {
                        width: parent.width
                        text: qsTr("How to display the trash in the dock")
                        color: settingsWindow.secondaryText
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }

                    Row {
                        spacing: 16

                        ExplorerIconCard {
                            selected: settingsWindow.trashIconStyle === "windows"
                            previewSource: dockModel.trashWindowsIconUrl()
                            title: qsTr("Default")
                            subtitle: "Windows"
                            onClicked: settingsWindow.trashIconStyle = "windows"
                        }

                        ExplorerIconCard {
                            selected: settingsWindow.trashIconStyle === "macos"
                            previewSource: dockModel.trashMacIconUrl()
                            title: "macOS"
                            subtitle: qsTr("Like on Mac")
                            onClicked: settingsWindow.trashIconStyle = "macos"
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: settingsWindow.sectionSeparator
                    }

                    RowLayout {
                        width: parent.width
                        spacing: 12

                        ColumnLayout {
                            spacing: 2
                            Layout.alignment: Qt.AlignVCenter
                            Layout.fillWidth: true
                            Text {
                                text: qsTr("Downloads folder in dock")
                                color: settingsWindow.primaryText
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                text: qsTr("Show the downloads folder next to the trash")
                                color: settingsWindow.secondaryText
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }

                        MacToggle {
                            id: showDownloadsToggle
                            darkTheme: settingsWindow.darkTheme
                            checked: taskbarController.showDownloadsInDock
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: showDownloadsToggle.checked = !showDownloadsToggle.checked
                        }
                    }
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
                    text: qsTr("Quit")
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
                    text: qsTr("Apply")
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
                        const shouldPinFromTaskbar = settingsWindow.pinFromTaskbarRequested
                        taskbarController.apply(hideTaskbarToggle.checked, keepTaskbarAutoHideOnExitToggle.checked, showTopBarToggle.checked, Math.round(iconSizeSlider.value), hoverBounceToggle.checked, dragFadeToggle.checked, staticIconsToggle.checked, separateTransientToggle.checked, taskbarController.darkTheme, startWithWindowsToggle.checked, settingsWindow.explorerIconStyle, settingsWindow.trashIconStyle, settingsWindow.menuBarIconStyle, showDownloadsToggle.checked, settingsWindow.dockLightStyle);
                        if (shouldPinFromTaskbar)
                            dockModel.syncFromWindowsTaskbarPins()
                        settingsWindow.pinFromTaskbarRequested = false
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
            border.color: settingsWindow.outerBorder
            radius: 14
            z: 100
        }
    }
}

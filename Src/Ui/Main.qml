// 文件用途: 抽屉主窗口, 悬浮窗与搜索窗, 支持抽屉切换, 窗口管理与窗口详情
import QtQuick
import QtQuick.Effects

// 悬浮窗: 只负责快速切换抽屉, 由托盘菜单控制显示
Window {
    id: window
    width: 150
    height: 52 + (window.drawers.length > 0 ? window.drawers.length * 41 - 5 : 0)
    visible: false
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"
    title: qsTr("DrawDesk 悬浮窗")

    // 抽屉列表与活动索引来自抽屉服务
    readonly property var drawers: drawerService.drawerNames
    property int currentIndex: drawerService.activeIndex

    // 主窗口与搜索窗口实例, 按需创建
    property var managerWindowRef: null
    property bool floatingShownOnce: false

    readonly property bool mainWindowVisible: managerWindowRef !== null && managerWindowRef.visible

    function openMainWindow() {
        if (!managerWindowRef)
            managerWindowRef = mainWindowComponent.createObject(null);
        if (!managerWindowRef)
            return;

        managerWindowRef.selectedIndex = window.currentIndex >= 0 ? window.currentIndex : 0;
        managerWindowRef.resetSearch();
        managerWindowRef.refreshDetails();
        managerWindowRef.show();
        managerWindowRef.raise();
        managerWindowRef.requestActivate();
    }

    function quitApplication() {
        if (managerWindowRef)
            managerWindowRef.readyToQuit = true;
        drawerService.QuitApplication();
    }

    function toggleFloatingWindow() {
        if (window.visible) {
            drawerService.SaveFloatingPosition(window.x, window.y);
            window.visible = false;
            return;
        }

        if (floatingShownOnce) {
            window.visible = true;
            return;
        }

        floatingShownOnce = true;
        floatShowTimer.start();
    }

    Component.onCompleted: {
        var savedX = drawerService.FloatingWindowX();
        var savedY = drawerService.FloatingWindowY();
        x = savedX >= 0 ? savedX : Math.round((screen.width - width) / 2);
        y = savedY >= 0 ? savedY : 24;
    }

    Timer {
        id: floatShowTimer
        interval: 150
        onTriggered: window.visible = true
    }

    Rectangle {
        id: bar
        anchors.fill: parent
        radius: 14
        color: "#FFFFFF"
        border.width: 1
        border.color: "#E5E7EB"

        // 标题行
        Item {
            id: header
            anchors.top: parent.top
            anchors.topMargin: 10
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.right: parent.right
            anchors.rightMargin: 10
            height: 26

            Image {
                id: logo
                source: "Src/Ui/AppIcon.svg"
                width: 18
                height: 18
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                smooth: true
            }

            Text {
                text: qsTr("窗口抽屉")
                anchors.left: logo.right
                anchors.leftMargin: 7
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: 13
                font.bold: true
                color: "#1F2328"
            }
        }

        Column {
            id: column
            anchors.top: header.bottom
            anchors.topMargin: 6
            anchors.left: parent.left
            anchors.leftMargin: 10
            spacing: 5

            Repeater {
                model: window.drawers

                delegate: Item {
                    id: rowItem
                    width: 130
                    height: 36
                    property bool selected: index === window.currentIndex

                    Rectangle {
                        id: cardSource
                        anchors.fill: parent
                        radius: 10
                        color: rowItem.selected ? "#FFFFFF" : "#F3F4F6"
                        visible: false
                    }

                    MultiEffect {
                        source: cardSource
                        anchors.fill: cardSource
                        shadowEnabled: true
                        shadowColor: "#59000000"
                        shadowBlur: 0.5
                        shadowVerticalOffset: 3
                        shadowHorizontalOffset: 0
                        shadowOpacity: rowItem.selected ? 0.5 : 0.0

                        Behavior on shadowOpacity {
                            NumberAnimation { duration: 180 }
                        }
                    }

                    Row {
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 7

                        Rectangle {
                            width: 5
                            height: 5
                            radius: 2.5
                            anchors.verticalCenter: parent.verticalCenter
                            color: rowItem.selected ? "#4C8DFF" : "#C9CDD3"

                            Behavior on color {
                                ColorAnimation { duration: 160 }
                            }
                        }

                        Text {
                            text: modelData
                            font.pixelSize: 13
                            anchors.verticalCenter: parent.verticalCenter
                            color: rowItem.selected ? "#111827" : "#6B7280"

                            Behavior on color {
                                ColorAnimation { duration: 160 }
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: drawerService.SwitchTo(index)
                    }
                }
            }
        }

    }

    DragHandler {
        target: null
        grabPermissions: PointerHandler.CanTakeOverFromAnything

        onActiveChanged: {
            if (active)
                window.startSystemMove();
            else
                floatingPosTimer.restart();
        }
    }

    Timer {
        id: floatingPosTimer
        interval: 500
        onTriggered: drawerService.SaveFloatingPosition(window.x, window.y)
    }

    // 主窗口: 抽屉管理, 窗口详情与切换
    Component {
        id: mainWindowComponent

        Window {
            id: managerWindow
            flags: Qt.Window
            width: {
                var savedWidth = drawerService.ManagerWindowWidth();
                return savedWidth > 200 ? savedWidth : 580;
            }
            height: {
                var savedHeight = drawerService.ManagerWindowHeight();
                return savedHeight > 150 ? savedHeight : 460;
            }
            minimumWidth: 580
            minimumHeight: 380
            visible: false
            title: qsTr("DrawDesk - 抽屉管理")
            color: "#F7F8FA"

            property int selectedIndex: 0
            property int selectedRuleIndex: -1
            property bool ruleUseRegex: false
            property var windowDetails: []
            property var previewRects: []
            property var monitorLayout: []
            property bool renaming: false
            property bool confirmDelete: false
            property bool readyToQuit: false
            property string hintText: ''
            property string settingsMessage: ''

            property var searchResults: []
            property string searchKeyword: ""
            readonly property bool searchActive: searchKeyword.length > 0

            property bool todoMode: false
            property var todoItems: []
            property int pendingTodoCount: 0

            function resetSearch() {
                searchKeyword = "";
                searchResults = [];
                if (searchInput.text.length > 0)
                    searchInput.text = "";
            }

            function addSearchResult(resultIndex) {
                drawerService.AddSearchResultToDrawer(resultIndex, selectedIndex);
                searchResults = searchKeyword.length > 0
                        ? drawerService.SearchWindows(searchKeyword) : [];
                refreshDetails();
            }

            function refreshTodos() {
                if (selectedIndex >= 0 && selectedIndex < drawerService.drawerNames.length) {
                    todoItems = drawerService.TodoItems(selectedIndex);
                    pendingTodoCount = drawerService.PendingTodoCount(selectedIndex);
                } else {
                    todoItems = [];
                    pendingTodoCount = 0;
                }
            }

            function addTodo() {
                if (drawerService.AddTodo(selectedIndex, todoInput.text))
                    todoInput.text = "";
            }

            function toggleTodo(todoIndex, done) {
                drawerService.SetTodoDone(selectedIndex, todoIndex, done);
            }

            function updateTodo(todoIndex, text) {
                if (!drawerService.UpdateTodoText(selectedIndex, todoIndex, text))
                    Qt.callLater(function() { refreshTodos(); });
            }

            function removeTodo(todoIndex) {
                drawerService.RemoveTodo(selectedIndex, todoIndex);
            }

            function refreshHotkeyInput() {
                if (selectedIndex >= 0 && selectedIndex < drawerService.drawerNames.length)
                    drawerHotkeyInput.text = drawerService.DrawerHotkey(selectedIndex);
                else
                    drawerHotkeyInput.text = "";
            }

            function showHint(message) {
                hintText = message;
                hintTimer.restart();
            }

            function refreshDetails() {
                monitorLayout = drawerService.MonitorLayout();
                if (searchKeyword.length > 0)
                    searchResults = drawerService.SearchWindows(searchKeyword);
                if (selectedIndex >= 0 && selectedIndex < drawerService.drawerNames.length) {
                    windowDetails = drawerService.DrawerWindowDetails(selectedIndex);
                    previewRects = drawerService.DrawerPreview(selectedIndex);
                } else {
                    windowDetails = [];
                    previewRects = [];
                }
                refreshTodos();
            }

            function commitRename() {
                if (!renaming || selectedIndex < 0
                        || selectedIndex >= drawerService.drawerNames.length)
                    return;

                drawerService.RenameDrawer(selectedIndex, mainRenameInput.text);
                renaming = false;
            }

            onSelectedIndexChanged: {
                selectedRuleIndex = -1;
                if (renaming && selectedIndex >= 0
                        && selectedIndex < drawerService.drawerNames.length)
                    mainRenameInput.text = drawerService.drawerNames[selectedIndex];
                refreshDetails();
                refreshHotkeyInput();
                refreshTodos();
            }
            onVisibleChanged: {
                if (visible) {
                    refreshDetails();
                    refreshHotkeyInput();
                    refreshTodos();
                }
            }

            Component.onCompleted: {
                x = Math.round((screen.width - width) / 2);
                y = Math.round((screen.height - height) / 2);
            }

            Timer {
                id: sizeSaveTimer
                interval: 800
                onTriggered: drawerService.SaveManagerWindowSize(managerWindow.width, managerWindow.height)
            }

            Timer {
                id: hintTimer
                interval: 2500
                onTriggered: managerWindow.hintText = ''
            }

            onWidthChanged: sizeSaveTimer.restart()
            onHeightChanged: sizeSaveTimer.restart()

            // 关闭窗口时弹出确认
            onClosing: function(close) {
                if (managerWindow.readyToQuit) {
                    close.accepted = true;
                    return;
                }
                close.accepted = false;
                closeLayer.visible = true;
            }

            Connections {
                target: drawerService

                function onDrawersChanged() {
                    if (managerWindow.selectedIndex >= drawerService.drawerNames.length)
                        managerWindow.selectedIndex =
                            Math.max(0, drawerService.drawerNames.length - 1);
                    managerWindow.refreshDetails();
                    managerWindow.refreshHotkeyInput();
                    managerWindow.refreshTodos();
                }

                function onTodosChanged() {
                    Qt.callLater(function() { managerWindow.refreshTodos(); });
                }

            }

            Row {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                // 左侧抽屉列表
                Rectangle {
                    width: 160
                    height: parent.height
                    radius: 10
                    color: "#FFFFFF"
                    border.width: 1
                    border.color: "#E5E7EB"

                    Text {
                        id: drawerListTitle
                        anchors.top: parent.top
                        anchors.topMargin: 10
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        text: qsTr("抽屉")
                        font.pixelSize: 12
                        font.bold: true
                        color: "#1F2328"
                    }

                    ListView {
                        id: drawerListView
                        anchors.top: drawerListTitle.bottom
                        anchors.topMargin: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: restoreAllRow.top
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        anchors.bottomMargin: 8
                        spacing: 3
                        clip: true
                        model: drawerService.drawerNames

                        delegate: Rectangle {
                            width: drawerListView.width
                            height: 30
                            radius: 7
                            color: index === managerWindow.selectedIndex
                                   ? "#EAF1FF"
                                   : (index === drawerService.activeIndex ? "#F3F4F6" : "transparent")

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 76
                                text: (index === drawerService.activeIndex ? "● " : "") + modelData
                                font.pixelSize: 12
                                color: index === managerWindow.selectedIndex ? "#2563EB" : "#4B5563"
                                elide: Text.ElideRight
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    managerWindow.renaming = false;
                                    managerWindow.confirmDelete = false;
                                    managerWindow.selectedIndex = index;
                                }
                            }

                            Row {
                                anchors.right: parent.right
                                anchors.rightMargin: 4
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2

                                Rectangle {
                                    width: 18
                                    height: 18
                                    radius: 5
                                    color: "#E5E7EB"

                                    Text {
                                        anchors.centerIn: parent
                                        text: "↑"
                                        font.pixelSize: 11
                                        color: "#4B5563"
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: drawerService.MoveDrawerUp(index)
                                    }
                                }

                                Rectangle {
                                    width: 18
                                    height: 18
                                    radius: 5
                                    color: "#E5E7EB"

                                    Text {
                                        anchors.centerIn: parent
                                        text: "↓"
                                        font.pixelSize: 11
                                        color: "#4B5563"
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: drawerService.MoveDrawerDown(index)
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        id: restoreAllRow
                        anchors.bottom: addDrawerRow.top
                        anchors.bottomMargin: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        height: 26
                        radius: 7
                        color: "#F3F4F6"

                        Text {
                            anchors.centerIn: parent
                            text: qsTr("显示全部")
                            font.pixelSize: 11
                            color: "#4B5563"
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                drawerService.RestoreAll();
                                managerWindow.refreshDetails();
                            }
                        }
                    }

                    Row {
                        id: addDrawerRow
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 4

                        Rectangle {
                            width: parent.width - 46
                            height: 26
                            radius: 7
                            color: "#F7F8FA"
                            border.width: 1
                            border.color: "#E5E7EB"

                            TextInput {
                                id: mainNewDrawerInput
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                verticalAlignment: TextInput.AlignVCenter
                                font.pixelSize: 12
                                color: "#1F2328"
                                clip: true

                                onAccepted: {
                                    drawerService.AddDrawer(text);
                                    text = "";
                                }
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                visible: mainNewDrawerInput.text.length === 0
                                text: qsTr("新抽屉")
                                font.pixelSize: 12
                                color: "#B0B5BA"
                            }
                        }

                        Rectangle {
                            width: 40
                            height: 26
                            radius: 7
                            color: "#EAF1FF"

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("添加")
                                font.pixelSize: 11
                                color: "#2563EB"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    drawerService.AddDrawer(mainNewDrawerInput.text);
                                    mainNewDrawerInput.text = "";
                                }
                            }
                        }
                    }
                }

                // 右侧窗口详情
                Rectangle {
                    width: parent.width - 172
                    height: parent.height
                    radius: 10
                    color: "#FFFFFF"
                    border.width: 1
                    border.color: "#E5E7EB"

                    Item {
                        id: detailHeader
                        anchors.top: parent.top
                        anchors.topMargin: 10
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        height: 28

                        Text {
                            visible: !managerWindow.renaming
                            anchors.left: parent.left
                            anchors.right: viewTabs.left
                            anchors.rightMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            elide: Text.ElideRight
                            text: managerWindow.selectedIndex >= 0
                                  && managerWindow.selectedIndex < drawerService.drawerNames.length
                                  ? drawerService.drawerNames[managerWindow.selectedIndex]
                                  : ""
                            font.pixelSize: 14
                            font.bold: true
                            color: "#1F2328"
                        }

                        TextInput {
                            id: mainRenameInput
                            visible: managerWindow.renaming
                            anchors.left: parent.left
                            anchors.right: viewTabs.left
                            anchors.rightMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            font.pixelSize: 13
                            color: "#1F2328"
                            selectByMouse: true
                            clip: true

                            onAccepted: managerWindow.commitRename()

                            onActiveFocusChanged: {
                                if (!activeFocus && managerWindow.renaming)
                                    managerWindow.commitRename();
                            }
                        }

                        Row {
                            id: viewTabs
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 4

                            Rectangle {
                                width: 44
                                height: 22
                                radius: 6
                                color: managerWindow.todoMode ? "#F3F4F6" : "#EAF1FF"

                                Text {
                                    anchors.centerIn: parent
                                    text: qsTr("窗口")
                                    font.pixelSize: 11
                                    color: managerWindow.todoMode ? "#4B5563" : "#2563EB"
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        managerWindow.todoMode = false;
                                        managerWindow.refreshDetails();
                                        managerWindow.refreshTodos();
                                    }
                                }
                            }

                            Rectangle {
                                width: 74
                                height: 22
                                radius: 6
                                color: managerWindow.todoMode ? "#EAF1FF" : "#F3F4F6"

                                Text {
                                    anchors.centerIn: parent
                                    text: qsTr("待办") + " (" + managerWindow.pendingTodoCount + ")"
                                    font.pixelSize: 11
                                    color: managerWindow.todoMode ? "#2563EB" : "#4B5563"
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        managerWindow.todoMode = true;
                                        managerWindow.refreshTodos();
                                    }
                                }
                            }
                        }

                    }

                    // 抽屉操作与快捷键配置
                    Row {
                        id: drawerHotkeyRow
                        anchors.top: detailHeader.bottom
                        anchors.topMargin: 6
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        height: 24
                        spacing: 4

                        Rectangle {
                            width: 38
                            height: 22
                            radius: 6
                            color: "#EAF1FF"

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("切换")
                                font.pixelSize: 11
                                color: "#2563EB"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: drawerService.SwitchTo(managerWindow.selectedIndex)
                            }
                        }

                        Rectangle {
                            width: 40
                            height: 22
                            radius: 6
                            color: "#F3F4F6"

                            Text {
                                anchors.centerIn: parent
                                text: managerWindow.renaming ? qsTr("完成") : qsTr("改名")
                                font.pixelSize: 11
                                color: managerWindow.renaming ? "#2563EB" : "#4B5563"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (managerWindow.renaming) {
                                        managerWindow.commitRename();
                                        return;
                                    }
                                    if (managerWindow.selectedIndex < 0
                                            || managerWindow.selectedIndex
                                               >= drawerService.drawerNames.length) {
                                        managerWindow.showHint("请先选择一个抽屉");
                                        return;
                                    }
                                    managerWindow.confirmDelete = false;
                                    mainRenameInput.text = drawerService.drawerNames[
                                                managerWindow.selectedIndex];
                                    managerWindow.renaming = true;
                                    mainRenameInput.forceActiveFocus();
                                }
                            }
                        }

                        Rectangle {
                            width: 38
                            height: 22
                            radius: 6
                            color: managerWindow.confirmDelete ? "#FDE8E8" : "#F3F4F6"

                            Text {
                                anchors.centerIn: parent
                                text: managerWindow.confirmDelete ? qsTr("确认") : qsTr("删除")
                                font.pixelSize: 11
                                color: managerWindow.confirmDelete ? "#DC2626" : "#4B5563"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (managerWindow.confirmDelete) {
                                        drawerService.RemoveDrawer(managerWindow.selectedIndex);
                                        managerWindow.confirmDelete = false;
                                    } else {
                                        managerWindow.confirmDelete = true;
                                    }
                                }
                            }
                        }

                        Rectangle {
                            width: 40
                            height: 22
                            radius: 6
                            color: "#F3F4F6"

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("规则")
                                font.pixelSize: 11
                                color: "#4B5563"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (managerWindow.selectedRuleIndex < 0) {
                                        managerWindow.showHint("请先在列表中选择一条窗口条目");
                                        return;
                                    }
                                    ruleProcessInput.text = drawerService.RuleProcess(
                                                managerWindow.selectedIndex,
                                                managerWindow.selectedRuleIndex);
                                    ruleTitleInput.text = drawerService.RuleTitle(
                                                managerWindow.selectedIndex,
                                                managerWindow.selectedRuleIndex);
                                    var pid = drawerService.RuleProcessId(
                                                managerWindow.selectedIndex,
                                                managerWindow.selectedRuleIndex);
                                    managerWindow.ruleUseRegex = drawerService.RuleUseRegex(
                                                managerWindow.selectedIndex,
                                                managerWindow.selectedRuleIndex);
                                    var livePid = drawerService.LiveProcessIdForRule(
                                                managerWindow.selectedIndex,
                                                managerWindow.selectedRuleIndex);
                                    // 优先显示当前窗口 PID, 与进程名和标题一样直接填入输入框
                                    rulePidInput.text = livePid > 0 ? livePid : (pid > 0 ? pid : "");
                                    ruleEditLayer.visible = true;
                                }
                            }
                        }

                        Rectangle {
                            width: 40
                            height: 22
                            radius: 6
                            color: "#F3F4F6"

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("设置")
                                font.pixelSize: 11
                                color: "#4B5563"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    hotkeyAddInput.text = drawerService.HotkeyAddWindow();
                                    hotkeyRemoveInput.text = drawerService.HotkeyRemoveWindow();
                                    managerWindow.settingsMessage = "";
                                    settingsLayer.visible = true;
                                }
                            }
                        }

                        Item {
                            width: 4
                            height: 1
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Ctrl+Alt+"
                            font.pixelSize: 10
                            color: "#6B7280"
                        }

                        Rectangle {
                            width: 44
                            height: 22
                            radius: 6
                            color: "#F7F8FA"
                            border.width: 1
                            border.color: "#E5E7EB"

                            TextInput {
                                id: drawerHotkeyInput
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                anchors.rightMargin: 6
                                verticalAlignment: TextInput.AlignVCenter
                                font.pixelSize: 11
                                color: "#1F2328"
                                maximumLength: 8

                                onAccepted: drawerService.SaveDrawerHotkey(
                                                managerWindow.selectedIndex, text)
                            }
                        }

                        Rectangle {
                            width: 34
                            height: 22
                            radius: 6
                            color: "#EAF1FF"

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("保存")
                                font.pixelSize: 11
                                color: "#2563EB"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: drawerService.SaveDrawerHotkey(
                                               managerWindow.selectedIndex, drawerHotkeyInput.text)
                            }
                        }
                    }
                    // 位置预览: 显示器与窗口的虚拟分布
                    Rectangle {
                        id: previewBox
                        visible: !managerWindow.todoMode
                        anchors.top: drawerHotkeyRow.bottom
                        anchors.topMargin: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        height: 140
                        radius: 8
                        color: "#F7F8FA"
                        border.width: 1
                        border.color: "#E5E7EB"
                        clip: true

                        Item {
                            id: canvas
                            anchors.fill: parent
                            anchors.margins: 8

                            readonly property var monitors: managerWindow.monitorLayout
                            readonly property var rects: managerWindow.previewRects

                            readonly property real minX: {
                                var v = Infinity;
                                for (var i = 0; i < monitors.length; ++i)
                                    v = Math.min(v, monitors[i].x);
                                return isFinite(v) ? v : 0;
                            }
                            readonly property real minY: {
                                var v = Infinity;
                                for (var i = 0; i < monitors.length; ++i)
                                    v = Math.min(v, monitors[i].y);
                                return isFinite(v) ? v : 0;
                            }
                            readonly property real maxX: {
                                var v = -Infinity;
                                for (var i = 0; i < monitors.length; ++i)
                                    v = Math.max(v, monitors[i].x + monitors[i].w);
                                return isFinite(v) ? v : 1;
                            }
                            readonly property real maxY: {
                                var v = -Infinity;
                                for (var i = 0; i < monitors.length; ++i)
                                    v = Math.max(v, monitors[i].y + monitors[i].h);
                                return isFinite(v) ? v : 1;
                            }
                            readonly property real viewWidth: Math.max(1, maxX - minX)
                            readonly property real viewHeight: Math.max(1, maxY - minY)
                            readonly property real fitScale: Math.min(width / viewWidth,
                                                                     height / viewHeight)
                            readonly property real offsetX: (width - viewWidth * fitScale) / 2
                                                            - minX * fitScale
                            readonly property real offsetY: (height - viewHeight * fitScale) / 2
                                                            - minY * fitScale

                            function mapX(value) { return offsetX + value * fitScale }
                            function mapY(value) { return offsetY + value * fitScale }

                            Repeater {
                                model: canvas.monitors

                                delegate: Item {
                                    x: canvas.mapX(modelData.x)
                                    y: canvas.mapY(modelData.y)
                                    width: Math.max(2, modelData.w * canvas.fitScale)
                                    height: Math.max(2, modelData.h * canvas.fitScale)

                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 4
                                        color: "#FFFFFF"
                                        border.width: 1
                                        border.color: "#CBD1D8"
                                    }

                                    Text {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.margins: 3
                                        text: modelData.name
                                        font.pixelSize: 9
                                        color: "#9AA0A6"
                                    }
                                }
                            }

                            Repeater {
                                model: canvas.rects

                                delegate: Rectangle {
                                    x: canvas.mapX(modelData.x)
                                    y: canvas.mapY(modelData.y)
                                    width: Math.max(6, modelData.w * canvas.fitScale)
                                    height: Math.max(6, modelData.h * canvas.fitScale)
                                    radius: 3
                                    color: modelData.ruleIndex === managerWindow.selectedRuleIndex
                                           ? "#77F59E0B" : "#554C8DFF"
                                    border.width: (modelData.ruleIndex
                                                   === managerWindow.selectedRuleIndex) ? 2 : 1
                                    border.color: modelData.ruleIndex === managerWindow.selectedRuleIndex
                                                  ? "#F59E0B" : "#4C8DFF"
                                }
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: canvas.monitors.length === 0
                                text: qsTr("未获取到显示器信息")
                                font.pixelSize: 10
                                color: "#9AA0A6"
                            }
                        }
                    }

                    // 待办输入
                    Row {
                        id: todoInputRow
                        visible: managerWindow.todoMode
                        anchors.top: drawerHotkeyRow.bottom
                        anchors.topMargin: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        height: 26
                        spacing: 6

                        Rectangle {
                            width: parent.width - 52
                            height: 26
                            radius: 7
                            color: "#F7F8FA"
                            border.width: 1
                            border.color: "#E5E7EB"

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                visible: todoInput.text.length === 0
                                text: qsTr("新增待办, 回车或点添加")
                                font.pixelSize: 11
                                color: "#9AA0A6"
                            }

                            TextInput {
                                id: todoInput
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                verticalAlignment: TextInput.AlignVCenter
                                font.pixelSize: 12
                                color: "#1F2328"
                                selectByMouse: true
                                clip: true
                                onAccepted: managerWindow.addTodo()
                            }
                        }

                        Rectangle {
                            width: 46
                            height: 26
                            radius: 7
                            color: "#EAF1FF"

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("添加")
                                font.pixelSize: 11
                                color: "#2563EB"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: managerWindow.addTodo()
                            }
                        }
                    }

                    // 待办列表
                    ListView {
                        id: todoListView
                        visible: managerWindow.todoMode
                        anchors.top: todoInputRow.bottom
                        anchors.topMargin: 6
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        anchors.bottomMargin: 12
                        spacing: 4
                        clip: true
                        model: managerWindow.todoItems

                        delegate: Item {
                            width: todoListView.width
                            height: 28
                            property bool isDone: modelData.done

                            Rectangle {
                                anchors.fill: parent
                                radius: 7
                                color: isDone ? "#F7F8FA" : "#FFFFFF"
                                border.width: 1
                                border.color: "#E5E7EB"
                            }

                            Rectangle {
                                width: 16
                                height: 16
                                radius: 4
                                anchors.left: parent.left
                                anchors.leftMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                color: isDone ? "#2563EB" : "#FFFFFF"
                                border.width: 1
                                border.color: isDone ? "#2563EB" : "#D1D5DB"

                                Text {
                                    anchors.centerIn: parent
                                    visible: isDone
                                    text: "✓"
                                    font.pixelSize: 10
                                    color: "#FFFFFF"
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: managerWindow.toggleTodo(modelData.index, !isDone)
                                }
                            }

                            TextInput {
                                id: todoTextInput
                                anchors.left: parent.left
                                anchors.leftMargin: 28
                                anchors.right: todoRemoveButton.left
                                anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.text
                                font.pixelSize: 12
                                font.strikeout: isDone
                                color: isDone ? "#9AA0A6" : "#1F2328"
                                selectByMouse: true
                                clip: true
                                onAccepted: focus = false
                                onEditingFinished: managerWindow.updateTodo(modelData.index, text)
                            }

                            Rectangle {
                                id: todoRemoveButton
                                width: 18
                                height: 18
                                radius: 5
                                anchors.right: parent.right
                                anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                color: "#FDE8E8"

                                Text {
                                    anchors.centerIn: parent
                                    text: "×"
                                    font.pixelSize: 12
                                    color: "#DC2626"
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: managerWindow.removeTodo(modelData.index)
                                }
                            }
                        }
                    }

                    Text {
                        visible: managerWindow.todoMode && todoListView.count === 0
                        anchors.top: todoInputRow.bottom
                        anchors.topMargin: 18
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("还没有待办")
                        font.pixelSize: 11
                        color: "#9AA0A6"
                    }

                    // 搜索框: 输入关键字后, 下方列表变为搜索结果
                    Rectangle {
                        id: searchBox
                        visible: !managerWindow.todoMode
                        anchors.top: previewBox.bottom
                        anchors.topMargin: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        height: 26
                        radius: 7
                        color: "#F7F8FA"
                        border.width: 1
                        border.color: "#E5E7EB"

                        TextInput {
                            id: searchInput
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            verticalAlignment: TextInput.AlignVCenter
                            font.pixelSize: 12
                            color: "#1F2328"
                            clip: true

                            onTextChanged: {
                                managerWindow.searchKeyword = text;
                                managerWindow.searchResults = text.length > 0
                                        ? drawerService.SearchWindows(text) : [];
                            }

                            Keys.onEscapePressed: {
                                text = "";
                                managerWindow.selectedRuleIndex = -1;
                            }
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            visible: searchInput.text.length === 0
                            text: qsTr("搜索窗口并加入当前抽屉")
                            font.pixelSize: 11
                            color: "#B0B5BA"
                        }
                    }

                    // 窗口清单, 未搜索时显示抽屉条目, 搜索时显示结果并可加入
                    ListView {
                        id: detailListView
                        visible: !managerWindow.todoMode
                        anchors.top: searchBox.bottom
                        anchors.topMargin: 6
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: bottomArea.top
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        anchors.bottomMargin: 8
                        spacing: 6
                        clip: true
                        model: managerWindow.searchActive
                               ? managerWindow.searchResults
                               : managerWindow.windowDetails

                        delegate: Item {
                            id: detailRow
                            width: detailListView.width
                            height: Math.max(detailText.height, 20) + 2
                            property bool selected: !managerWindow.searchActive
                                                    && index === managerWindow.selectedRuleIndex

                            Rectangle {
                                anchors.fill: parent
                                radius: 6
                                color: detailRow.selected ? "#EAF1FF" : "transparent"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (managerWindow.searchActive)
                                        drawerService.ActivateSearchResult(index);
                                    else
                                        managerWindow.selectedRuleIndex =
                                                detailRow.selected ? -1 : index;
                                }
                            }

                            Text {
                                id: detailText
                                anchors.left: parent.left
                                anchors.leftMargin: 6
                                width: parent.width - (managerWindow.searchActive ? 74 : 12)
                                text: modelData
                                font.pixelSize: 12
                                color: detailRow.selected ? "#2563EB" : "#4B5563"
                                wrapMode: Text.WordWrap
                                lineHeight: 1.2
                            }

                            Rectangle {
                                visible: managerWindow.searchActive
                                anchors.right: parent.right
                                anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                width: 54
                                height: 20
                                radius: 5
                                color: "#EAF1FF"

                                Text {
                                    anchors.centerIn: parent
                                    text: qsTr("加入")
                                    font.pixelSize: 10
                                    color: "#2563EB"
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: managerWindow.addSearchResult(index)
                                }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: !managerWindow.searchActive && managerWindow.windowDetails.length === 0
                            text: qsTr("这个抽屉还没有添加窗口")
                            font.pixelSize: 12
                            color: "#9AA0A6"
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: managerWindow.searchActive && managerWindow.searchResults.length === 0
                            text: qsTr("未找到匹配窗口")
                            font.pixelSize: 12
                            color: "#9AA0A6"
                        }
                    }

                    // 底部: 左侧操作按钮, 右侧状态消息
                    Row {
                        id: bottomArea
                        visible: !managerWindow.todoMode
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 10
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        Row {
                            id: bottomButtons
                            spacing: 6

                            Rectangle {
                                width: 62
                                height: 24
                                radius: 6
                                color: "#EAF1FF"

                                Text {
                                    anchors.centerIn: parent
                                    text: qsTr("加入窗口")
                                    font.pixelSize: 11
                                    color: "#2563EB"
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        drawerService.AddForegroundWindowToDrawer(
                                                    managerWindow.selectedIndex);
                                        managerWindow.refreshDetails();
                                    }
                                }
                            }

                            Rectangle {
                                width: 62
                                height: 24
                                radius: 6
                                color: "#F3F4F6"

                                Text {
                                    anchors.centerIn: parent
                                    text: qsTr("移出窗口")
                                    font.pixelSize: 11
                                    color: "#4B5563"
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        if (managerWindow.selectedRuleIndex >= 0) {
                                            drawerService.RemoveRule(
                                                        managerWindow.selectedIndex,
                                                        managerWindow.selectedRuleIndex);
                                            managerWindow.selectedRuleIndex = -1;
                                        } else {
                                            drawerService.RemoveForegroundWindowFromDrawer(
                                                        managerWindow.selectedIndex);
                                        }
                                        managerWindow.refreshDetails();
                                    }
                                }
                            }

                            Rectangle {
                                width: 62
                                height: 24
                                radius: 6
                                color: "#F3F4F6"

                                Text {
                                    anchors.centerIn: parent
                                    text: qsTr("抓取桌面")
                                    font.pixelSize: 11
                                    color: "#4B5563"
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        drawerService.CaptureDesktopToDrawer(
                                                    managerWindow.selectedIndex);
                                        managerWindow.refreshDetails();
                                    }
                                }
                            }

                            Rectangle {
                                width: 42
                                height: 24
                                radius: 6
                                color: "#F3F4F6"

                                Text {
                                    anchors.centerIn: parent
                                    text: qsTr("刷新")
                                    font.pixelSize: 11
                                    color: "#4B5563"
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        managerWindow.refreshDetails();
                                        managerWindow.showHint(qsTr("已刷新"));
                                    }
                                }
                            }
                        }

                        Text {
                            width: Math.max(0, parent.width - bottomButtons.width - bottomArea.spacing)
                            anchors.verticalCenter: parent.verticalCenter
                            horizontalAlignment: Text.AlignRight
                            text: managerWindow.hintText.length > 0
                                  ? managerWindow.hintText : drawerService.notice
                            font.pixelSize: 11
                            color: "#9AA0A6"
                            elide: Text.ElideRight
                        }
                    }

                }
            }
            // 规则编辑层
            Rectangle {
                id: ruleEditLayer
                visible: false
                anchors.fill: parent
                color: "#66000000"
                z: 210

                MouseArea {
                    anchors.fill: parent
                    onClicked: ruleEditLayer.visible = false
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 320
                    height: 240
                    radius: 12
                    color: "#FFFFFF"

                    // 拦截点击, 避免穿透到遮罩层导致误关闭
                    MouseArea {
                        anchors.fill: parent
                    }

                    Text {
                        anchors.top: parent.top
                        anchors.topMargin: 14
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        text: qsTr("编辑窗口规则")
                        font.pixelSize: 13
                        font.bold: true
                        color: "#1F2328"
                    }

                    Text {
                        anchors.top: parent.top
                        anchors.topMargin: 44
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        text: qsTr("进程名")
                        font.pixelSize: 11
                        color: "#6B7280"
                    }

                    Rectangle {
                        anchors.top: parent.top
                        anchors.topMargin: 60
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        height: 26
                        radius: 7
                        color: "#F7F8FA"
                        border.width: 1
                        border.color: "#E5E7EB"

                        TextInput {
                            id: ruleProcessInput
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            verticalAlignment: TextInput.AlignVCenter
                            font.pixelSize: 12
                            color: "#1F2328"
                            clip: true
                        }
                    }

                    Text {
                        anchors.top: parent.top
                        anchors.topMargin: 92
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        text: qsTr("标题匹配")
                        font.pixelSize: 11
                        color: "#6B7280"
                    }

                    Text {
                        anchors.top: parent.top
                        anchors.topMargin: 92
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        text: qsTr("正则: ") + (managerWindow.ruleUseRegex ? qsTr("开") : qsTr("关"))
                        font.pixelSize: 11
                        color: managerWindow.ruleUseRegex ? "#2563EB" : "#9AA0A6"

                        MouseArea {
                            anchors.fill: parent
                            onClicked: managerWindow.ruleUseRegex = !managerWindow.ruleUseRegex
                        }
                    }

                    Rectangle {
                        anchors.top: parent.top
                        anchors.topMargin: 108
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        height: 26
                        radius: 7
                        color: "#F7F8FA"
                        border.width: 1
                        border.color: "#E5E7EB"

                        TextInput {
                            id: ruleTitleInput
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            verticalAlignment: TextInput.AlignVCenter
                            font.pixelSize: 12
                            color: "#1F2328"
                            clip: true
                        }
                    }

                    Text {
                        anchors.top: parent.top
                        anchors.topMargin: 140
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        text: qsTr("PID 匹配, 可选, 0 或不填为不启用")
                        font.pixelSize: 11
                        color: "#6B7280"
                    }

                    Rectangle {
                        anchors.top: parent.top
                        anchors.topMargin: 156
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        height: 26
                        radius: 7
                        color: "#F7F8FA"
                        border.width: 1
                        border.color: "#E5E7EB"

                        TextInput {
                            id: rulePidInput
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            verticalAlignment: TextInput.AlignVCenter
                            font.pixelSize: 12
                            color: "#1F2328"
                            clip: true
                        }
                    }

                    Row {
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 14
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 10

                        Rectangle {
                            width: 76
                            height: 26
                            radius: 7
                            color: "#EAF1FF"

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("保存")
                                font.pixelSize: 11
                                color: "#2563EB"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    var pidValue = parseInt(rulePidInput.text);
                                    if (isNaN(pidValue))
                                        pidValue = 0;
                                    if (drawerService.UpdateRule(managerWindow.selectedIndex,
                                                                 managerWindow.selectedRuleIndex,
                                                                 ruleProcessInput.text,
                                                                 ruleTitleInput.text,
                                                                 pidValue,
                                                                 managerWindow.ruleUseRegex)) {
                                        ruleEditLayer.visible = false;
                                        managerWindow.refreshDetails();
                                    } else {
                                        ruleEditLayer.visible = false;
                                        managerWindow.showHint("保存失败: 进程名不能为空");
                                    }
                                }
                            }
                        }

                        Rectangle {
                            width: 76
                            height: 26
                            radius: 7
                            color: "#F3F4F6"

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("取消")
                                font.pixelSize: 11
                                color: "#4B5563"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: ruleEditLayer.visible = false
                            }
                        }
                    }
                }
            }

            // 设置层: 快捷键与配置文件
            Rectangle {
                id: settingsLayer
                visible: false
                anchors.fill: parent
                color: "#66000000"
                z: 220

                MouseArea {
                    anchors.fill: parent
                    onClicked: settingsLayer.visible = false
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 340
                    height: 210
                    radius: 12
                    color: "#FFFFFF"

                    // 拦截点击, 避免穿透到遮罩层导致误关闭
                    MouseArea {
                        anchors.fill: parent
                    }

                    Text {
                        anchors.top: parent.top
                        anchors.topMargin: 14
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        text: qsTr("设置")
                        font.pixelSize: 13
                        font.bold: true
                        color: "#1F2328"
                    }

                    Text {
                        anchors.top: parent.top
                        anchors.topMargin: 46
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        text: qsTr("加入窗口快捷键")
                        font.pixelSize: 11
                        color: "#6B7280"
                    }

                    Rectangle {
                        anchors.top: parent.top
                        anchors.topMargin: 62
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        height: 26
                        radius: 7
                        color: "#F7F8FA"
                        border.width: 1
                        border.color: "#E5E7EB"

                        TextInput {
                            id: hotkeyAddInput
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            verticalAlignment: TextInput.AlignVCenter
                            font.pixelSize: 12
                            color: "#1F2328"
                            clip: true
                        }
                    }

                    Text {
                        anchors.top: parent.top
                        anchors.topMargin: 94
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        text: qsTr("移出窗口快捷键")
                        font.pixelSize: 11
                        color: "#6B7280"
                    }

                    Rectangle {
                        anchors.top: parent.top
                        anchors.topMargin: 110
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        height: 26
                        radius: 7
                        color: "#F7F8FA"
                        border.width: 1
                        border.color: "#E5E7EB"

                        TextInput {
                            id: hotkeyRemoveInput
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            verticalAlignment: TextInput.AlignVCenter
                            font.pixelSize: 12
                            color: "#1F2328"
                            clip: true
                        }
                    }

                    Text {
                        anchors.top: parent.top
                        anchors.topMargin: 146
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        text: managerWindow.settingsMessage
                        font.pixelSize: 11
                        color: "#9AA0A6"
                        elide: Text.ElideRight
                    }
                    Row {
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 12
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 8

                        Rectangle {
                            width: 64
                            height: 26
                            radius: 7
                            color: "#EAF1FF"

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("保存")
                                font.pixelSize: 11
                                color: "#2563EB"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (drawerService.SaveWindowHotkeys(hotkeyAddInput.text,
                                                                        hotkeyRemoveInput.text)) {
                                        settingsLayer.visible = false;
                                    } else {
                                        managerWindow.settingsMessage = drawerService.notice.length > 0
                                                ? drawerService.notice
                                                : "快捷键格式无效或重复, 请检查";
                                    }
                                }
                            }
                        }

                        Rectangle {
                            width: 96
                            height: 26
                            radius: 7
                            color: "#F3F4F6"

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("打开配置文件")
                                font.pixelSize: 11
                                color: "#4B5563"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: drawerService.OpenConfigFolder()
                            }
                        }

                        Rectangle {
                            width: 64
                            height: 26
                            radius: 7
                            color: "#F3F4F6"

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("关闭")
                                font.pixelSize: 11
                                color: "#4B5563"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: settingsLayer.visible = false
                            }
                        }
                    }
                }
            }

            // 关闭确认层
            Rectangle {
                id: closeLayer
                visible: false
                anchors.fill: parent
                color: "#66000000"
                z: 200

                MouseArea {
                    anchors.fill: parent
                    onClicked: closeLayer.visible = false
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 250
                    height: 116
                    radius: 12
                    color: "#FFFFFF"

                    // 拦截点击, 避免穿透到遮罩层导致误关闭
                    MouseArea {
                        anchors.fill: parent
                    }

                    Text {
                        anchors.top: parent.top
                        anchors.topMargin: 16
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("要关闭 DrawDesk 吗?")
                        font.pixelSize: 13
                        font.bold: true
                        color: "#1F2328"
                    }

                    Row {
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 16
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 10

                        Rectangle {
                            width: 104
                            height: 26
                            radius: 7
                            color: "#EAF1FF"

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("最小化到托盘")
                                font.pixelSize: 11
                                color: "#2563EB"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    console.log("ui: minimize to tray");
                                    closeLayer.visible = false;
                                    managerWindow.hide();
                                }
                            }
                        }

                        Rectangle {
                            width: 104
                            height: 26
                            radius: 7
                            color: "#FDE8E8"

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("退出程序")
                                font.pixelSize: 11
                                color: "#DC2626"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    managerWindow.readyToQuit = true;
                                    drawerService.QuitApplication();
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
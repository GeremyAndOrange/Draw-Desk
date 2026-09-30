// 文件用途: 单元测试实现, 使用 Qt Test 验证不依赖界面的核心逻辑
#include "DrawDeskTests.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "Models/Drawer.h"
#include "Platform/ConfigStore.h"
#include "Platform/HotkeyService.h"
#include "Platform/WindowApi.h"

void DrawDeskTests::hotkeyParsing()
{
    UINT modifiers = 0;
    QVERIFY(DrawDesk::HotkeyService::ParseModifiers(QStringLiteral("Ctrl+Alt"), &modifiers));
    QCOMPARE(modifiers, static_cast<UINT>(MOD_CONTROL | MOD_ALT));
    QVERIFY(DrawDesk::HotkeyService::ParseModifiers(QStringLiteral("ctrl + shift"), &modifiers));
    QVERIFY(!DrawDesk::HotkeyService::ParseModifiers(QStringLiteral("Foo"), &modifiers));

    UINT key = 0;
    QVERIFY(DrawDesk::HotkeyService::ParseHotkey(QStringLiteral("Ctrl+Alt+0"), &modifiers, &key));
    QCOMPARE(key, static_cast<UINT>('0'));

    QVERIFY(DrawDesk::HotkeyService::ParseHotkey(QStringLiteral("Ctrl+Alt+A"), &modifiers, &key));
    QCOMPARE(key, static_cast<UINT>('A'));

    QVERIFY(DrawDesk::HotkeyService::ParseHotkey(QStringLiteral("Ctrl+Alt+Space"), &modifiers, &key));
    QCOMPARE(key, static_cast<UINT>(VK_SPACE));

    QVERIFY(DrawDesk::HotkeyService::ParseHotkey(QStringLiteral("Ctrl+Alt+F12"), &modifiers, &key));
    QCOMPARE(key, static_cast<UINT>(VK_F12));

    QVERIFY(DrawDesk::HotkeyService::ParseHotkey(QStringLiteral("Ctrl+Alt+;"), &modifiers, &key));
    QCOMPARE(key, static_cast<UINT>(VK_OEM_1));

    QVERIFY(!DrawDesk::HotkeyService::ParseHotkey(QStringLiteral("Ctrl+Alt++"), &modifiers, &key));
    QVERIFY(!DrawDesk::HotkeyService::ParseHotkey(QStringLiteral("0"), &modifiers, &key));
}

void DrawDeskTests::ruleMatching()
{
    DrawDesk::WindowApi::WindowInfo info;
    info.processName = QStringLiteral("Code.exe");
    info.title = QStringLiteral("Project - Visual Studio Code");
    info.processId = 4321;

    DrawDesk::Models::WindowRule rule;
    rule.process = QStringLiteral("code.exe");
    QVERIFY(DrawDesk::WindowApi::MatchesRule(info, rule));

    rule.titlePattern = QStringLiteral("^Project");
    rule.useRegex = true;
    QVERIFY(DrawDesk::WindowApi::MatchesRule(info, rule));

    // 未显式开启正则时, 标题按字面匹配, 避免规则互相串窗口
    rule.useRegex = false;
    QVERIFY(!DrawDesk::WindowApi::MatchesRule(info, rule));

    rule.titlePattern = QStringLiteral("^Other");
    rule.useRegex = true;
    QVERIFY(!DrawDesk::WindowApi::MatchesRule(info, rule));

    rule.titlePattern.clear();
    rule.processId = 1234;
    QVERIFY(!DrawDesk::WindowApi::MatchesRule(info, rule));

    rule.processId = 4321;
    QVERIFY(DrawDesk::WindowApi::MatchesRule(info, rule));

    rule.processId = 9999;
    QVERIFY(!DrawDesk::WindowApi::MatchesRule(info, rule));
    rule.processId = 4321;

    rule.process.clear();
    QVERIFY(!DrawDesk::WindowApi::MatchesRule(info, rule));
}

void DrawDeskTests::configRoundTrip()
{
    // 数据目录放在构建目录内, 避开沙箱对系统临时目录的写入限制
    const QString dataDir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("TestData"));
    QDir(dataDir).removeRecursively();
    QVERIFY(QDir().mkpath(dataDir));
    qputenv("DRAWDESK_DATA_DIR", dataDir.toUtf8());

    DrawDesk::ConfigStore::EnsureDefaultConfig();
    QVERIFY(QFile::exists(DrawDesk::ConfigStore::ConfigPath()));

    const QVector<DrawDesk::Models::Drawer> defaults = DrawDesk::ConfigStore::LoadDrawers();
    QCOMPARE(defaults.size(), 5);
    QCOMPARE(defaults.at(0).hotkey, QStringLiteral("1"));
    QCOMPARE(defaults.at(4).hotkey, QStringLiteral("5"));

    DrawDesk::Models::Drawer drawer;
    drawer.id = QStringLiteral("test");
    drawer.name = QStringLiteral("测试抽屉");
    drawer.hotkey = QStringLiteral("A");

    DrawDesk::Models::WindowRule rule;
    rule.process = QStringLiteral("Test.exe");
    rule.titlePattern = QStringLiteral("^测试");
    rule.processId = 88;
    rule.useRegex = true;
    drawer.rules.append(rule);

    DrawDesk::Models::TodoItem pendingTodo;
    pendingTodo.text = QStringLiteral("完成登录页");
    drawer.todos.append(pendingTodo);

    DrawDesk::Models::TodoItem doneTodo;
    doneTodo.text = QStringLiteral("补充单元测试");
    doneTodo.done = true;
    drawer.todos.append(doneTodo);

    DrawDesk::Models::WindowLayout layout;
    layout.process = QStringLiteral("Test.exe");
    layout.titlePattern = QStringLiteral("^测试");
    layout.processId = 77;
    layout.rect = QRect(20, 30, 800, 600);
    layout.showCommand = 3;
    drawer.layout.append(layout);

    QVector<DrawDesk::Models::Drawer> drawers;
    drawers.append(drawer);
    QVERIFY(DrawDesk::ConfigStore::SaveDrawers(drawers));

    const QVector<DrawDesk::Models::Drawer> loaded = DrawDesk::ConfigStore::LoadDrawers();
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.at(0).id, QStringLiteral("test"));
    QCOMPARE(loaded.at(0).name, QStringLiteral("测试抽屉"));
    QCOMPARE(loaded.at(0).hotkey, QStringLiteral("A"));
    QCOMPARE(loaded.at(0).rules.size(), 1);
    QCOMPARE(loaded.at(0).rules.at(0).processId, 88);
    QVERIFY(loaded.at(0).rules.at(0).useRegex);
    QCOMPARE(loaded.at(0).todos.size(), 2);
    QCOMPARE(loaded.at(0).todos.at(0).text, QStringLiteral("完成登录页"));
    QVERIFY(!loaded.at(0).todos.at(0).done);
    QVERIFY(loaded.at(0).todos.at(1).done);

    QCOMPARE(loaded.at(0).layout.size(), 1);
    QCOMPARE(loaded.at(0).layout.at(0).processId, 77);
    QCOMPARE(loaded.at(0).layout.at(0).showCommand, 3);
    QCOMPARE(loaded.at(0).layout.at(0).rect, QRect(20, 30, 800, 600));

    DrawDesk::ConfigStore::SetManagerWindowSize(640, 480);
    QCOMPARE(DrawDesk::ConfigStore::ManagerWindowWidth(), 640);
    QCOMPARE(DrawDesk::ConfigStore::ManagerWindowHeight(), 480);

    DrawDesk::ConfigStore::SetFloatingPosition(12, 34);
    QCOMPARE(DrawDesk::ConfigStore::FloatingWindowX(), 12);
    QCOMPARE(DrawDesk::ConfigStore::FloatingWindowY(), 34);

    DrawDesk::ConfigStore::SetWindowHotkeys(QStringLiteral("Ctrl+Shift+Z"),
                                            QStringLiteral("Ctrl+Shift+X"));
    QCOMPARE(DrawDesk::ConfigStore::HotkeyAddWindow(), QStringLiteral("Ctrl+Shift+Z"));
    QCOMPARE(DrawDesk::ConfigStore::HotkeyRemoveWindow(), QStringLiteral("Ctrl+Shift+X"));

    QDir(dataDir).removeRecursively();
}

void DrawDeskTests::legacyConfigMigration()
{
    const QString dataDir =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("TestDataLegacy"));
    QDir(dataDir).removeRecursively();
    QVERIFY(QDir().mkpath(dataDir));
    qputenv("DRAWDESK_DATA_DIR", dataDir.toUtf8());

    QJsonArray drawers;
    for (int i = 0; i < 2; ++i) {
        QJsonObject drawer;
        drawer[QStringLiteral("id")] = QStringLiteral("drawer%1").arg(i);
        drawer[QStringLiteral("name")] = QStringLiteral("测试%1").arg(i);
        drawers.append(drawer);
    }

    QJsonObject root;
    root[QStringLiteral("schemaVersion")] = 1;
    root[QStringLiteral("drawers")] = drawers;

    QFile file(DrawDesk::ConfigStore::ConfigPath());
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(root).toJson());
    file.close();

    // 旧配置没有 hotkey 字段, 加载时按顺序补默认数字键
    const QVector<DrawDesk::Models::Drawer> loaded = DrawDesk::ConfigStore::LoadDrawers();
    QCOMPARE(loaded.size(), 2);
    QCOMPARE(loaded.at(0).hotkey, QStringLiteral("1"));
    QCOMPARE(loaded.at(1).hotkey, QStringLiteral("2"));

    QDir(dataDir).removeRecursively();
}
QTEST_MAIN(DrawDeskTests)
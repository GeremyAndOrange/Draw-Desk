// 文件用途: 配置模块实现, 负责配置读写与默认配置的原子写入
#include "Platform/ConfigStore.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QSaveFile>

#include "Platform/Log.h"

namespace {

// 构建默认配置骨架
QJsonObject DefaultRoot()
{
    QJsonObject settings;


    QJsonObject hotkeys;
    hotkeys[QStringLiteral("addWindow")] = QStringLiteral("Ctrl+Alt+Z");
    hotkeys[QStringLiteral("removeWindow")] = QStringLiteral("Ctrl+Alt+X");

    QJsonObject root;
    root[QStringLiteral("schemaVersion")] = 1;
    root[QStringLiteral("settings")] = settings;
    root[QStringLiteral("hotkeys")] = hotkeys;
    root[QStringLiteral("drawers")] = QJsonArray();
    return root;
}

// 读取配置对象, 不存在或损坏时返回默认骨架
QJsonObject ReadConfigObject()
{
    QFile file(DrawDesk::ConfigStore::ConfigPath());
    if (!file.open(QIODevice::ReadOnly))
        return DefaultRoot();

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        qWarning("配置文件格式无效, 使用默认配置");
        return DefaultRoot();
    }
    return document.object();
}

// 原子写入配置对象
bool WriteConfigObject(const QJsonObject &root)
{
    QSaveFile file(DrawDesk::ConfigStore::ConfigPath());
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning().noquote() << "配置写入失败:" << file.errorString();
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        qWarning().noquote() << "配置提交失败:" << file.errorString();
        return false;
    }
    return true;
}

// 读取 settings 中的整数值
int ReadSettingInt(const QString &key, int fallback)
{
    const QJsonObject root = ReadConfigObject();
    const QJsonObject settings = root.value(QStringLiteral("settings")).toObject();
    if (!settings.contains(key))
        return fallback;
    return settings.value(key).toInt(fallback);
}

// 读取 hotkeys 中的字符串值
QString ReadHotkeyString(const QString &key, const QString &fallback)
{
    const QJsonObject root = ReadConfigObject();
    const QJsonObject hotkeys = root.value(QStringLiteral("hotkeys")).toObject();
    if (!hotkeys.contains(key))
        return fallback;
    return hotkeys.value(key).toString(fallback);
}

// 一次写入多个 settings 整数值, 避免重复读写配置文件
void WriteSettingInts(const QVector<QPair<QString, int>> &values)
{
    QJsonObject root = ReadConfigObject();
    if (root.isEmpty())
        root = DefaultRoot();

    QJsonObject settings = root.value(QStringLiteral("settings")).toObject();
    for (const auto &pair : values)
        settings[pair.first] = pair.second;
    root[QStringLiteral("settings")] = settings;
    WriteConfigObject(root);
}

QString ShowCommandName(int command)
{
    switch (command) {
    case 2:
        return QStringLiteral("minimized");
    case 3:
        return QStringLiteral("maximized");
    default:
        return QStringLiteral("normal");
    }
}

// 把状态名转换为 Win32 显示命令值
int ShowCommandValue(const QString &name)
{
    if (name == QStringLiteral("minimized"))
        return 2;
    if (name == QStringLiteral("maximized"))
        return 3;
    return 1;
}

// 抽屉转 JSON
QJsonObject DrawerToJson(const DrawDesk::Models::Drawer &drawer)
{
    QJsonArray rules;
    for (const auto &rule : drawer.rules) {
        QJsonObject ruleObject;
        ruleObject[QStringLiteral("process")] = rule.process;
        ruleObject[QStringLiteral("titlePattern")] = rule.titlePattern;
        ruleObject[QStringLiteral("processId")] = rule.processId;
        ruleObject[QStringLiteral("titleRegex")] = rule.useRegex;
        rules.append(ruleObject);
    }

    QJsonArray layout;
    for (const auto &item : drawer.layout) {
        QJsonArray rect;
        rect.append(item.rect.x());
        rect.append(item.rect.y());
        rect.append(item.rect.width());
        rect.append(item.rect.height());

        QJsonObject layoutObject;
        layoutObject[QStringLiteral("process")] = item.process;
        layoutObject[QStringLiteral("titlePattern")] = item.titlePattern;
        layoutObject[QStringLiteral("processId")] = item.processId;
        layoutObject[QStringLiteral("rect")] = rect;
        layoutObject[QStringLiteral("showCommand")] = ShowCommandName(item.showCommand);
        layoutObject[QStringLiteral("titleRegex")] = item.useRegex;
        layout.append(layoutObject);
    }

    QJsonArray todos;
    for (const auto &todo : drawer.todos) {
        QJsonObject todoObject;
        todoObject[QStringLiteral("text")] = todo.text;
        todoObject[QStringLiteral("done")] = todo.done;
        todos.append(todoObject);
    }

    QJsonObject object;
    object[QStringLiteral("id")] = drawer.id;
    object[QStringLiteral("name")] = drawer.name;
    object[QStringLiteral("hotkey")] = drawer.hotkey;
    object[QStringLiteral("rules")] = rules;
    object[QStringLiteral("layout")] = layout;
    object[QStringLiteral("todos")] = todos;

    return object;
}

// JSON 转抽屉
DrawDesk::Models::Drawer DrawerFromJson(const QJsonObject &object)
{
    DrawDesk::Models::Drawer drawer;
    drawer.id = object.value(QStringLiteral("id")).toString();
    drawer.name = object.value(QStringLiteral("name")).toString();
    drawer.hotkey = object.value(QStringLiteral("hotkey")).toString().trimmed();

    for (const auto &ruleValue : object.value(QStringLiteral("rules")).toArray()) {
        const QJsonObject ruleObject = ruleValue.toObject();
        DrawDesk::Models::WindowRule rule;
        rule.process = ruleObject.value(QStringLiteral("process")).toString();
        rule.titlePattern = ruleObject.value(QStringLiteral("titlePattern")).toString();
        rule.processId = ruleObject.value(QStringLiteral("processId")).toInt(0);
        rule.useRegex = ruleObject.value(QStringLiteral("titleRegex")).toBool(false);
        if (!rule.process.isEmpty())
            drawer.rules.append(rule);
    }

    for (const auto &layoutValue : object.value(QStringLiteral("layout")).toArray()) {
        const QJsonObject layoutObject = layoutValue.toObject();
        const QJsonArray rect = layoutObject.value(QStringLiteral("rect")).toArray();
        if (rect.size() != 4)
            continue;

        DrawDesk::Models::WindowLayout item;
        item.process = layoutObject.value(QStringLiteral("process")).toString();
        item.titlePattern = layoutObject.value(QStringLiteral("titlePattern")).toString();
        item.processId = layoutObject.value(QStringLiteral("processId")).toInt(0);
        item.rect = QRect(rect.at(0).toInt(), rect.at(1).toInt(), rect.at(2).toInt(),
                          rect.at(3).toInt());
        item.showCommand = ShowCommandValue(layoutObject.value(QStringLiteral("showCommand")).toString());
        item.useRegex = layoutObject.value(QStringLiteral("titleRegex")).toBool(false);
        if (!item.process.isEmpty())
            drawer.layout.append(item);
    }

    for (const auto &todoValue : object.value(QStringLiteral("todos")).toArray()) {
        const QJsonObject todoObject = todoValue.toObject();
        DrawDesk::Models::TodoItem todo;
        todo.text = todoObject.value(QStringLiteral("text")).toString();
        todo.done = todoObject.value(QStringLiteral("done")).toBool(false);
        if (!todo.text.isEmpty())
            drawer.todos.append(todo);
    }

    if (drawer.name.isEmpty())
        drawer.name = drawer.id;
    return drawer;
}

}

namespace DrawDesk::ConfigStore {

QString ConfigPath()
{
    return Log::DataDir() + QStringLiteral("/Config.json");
}

QString CloakedStatePath()
{
    return Log::DataDir() + QStringLiteral("/Cloaked.json");
}

QVector<Models::Drawer> DefaultDrawers()
{
    QVector<Models::Drawer> drawers;
    const char *ids[] = { "coding", "debug", "docs", "chat", "other" };
    const char *names[] = { "编码", "调试", "文档", "通讯", "其他" };

    for (int i = 0; i < 5; ++i) {
        Models::Drawer drawer;
        drawer.id = QString::fromUtf8(ids[i]);
        drawer.name = QString::fromUtf8(names[i]);
        drawer.hotkey = QString::number(i + 1);
        drawers.append(drawer);
    }
    return drawers;
}

void EnsureDefaultConfig()
{
    // 配置不存在时生成完整默认配置
    if (!QFile::exists(ConfigPath())) {
        QJsonObject root = DefaultRoot();
        QJsonArray array;
        for (const auto &drawer : DefaultDrawers())
            array.append(DrawerToJson(drawer));
        root[QStringLiteral("drawers")] = array;

        if (WriteConfigObject(root))
            qInfo().noquote() << "已生成默认配置:" << ConfigPath();
        return;
    }

    // 旧配置缺少抽屉时补齐默认抽屉, 保留其他字段
    QJsonObject root = ReadConfigObject();
    if (root.value(QStringLiteral("drawers")).toArray().isEmpty()) {
        QJsonArray array;
        for (const auto &drawer : DefaultDrawers())
            array.append(DrawerToJson(drawer));
        root[QStringLiteral("drawers")] = array;
        if (WriteConfigObject(root))
            qInfo().noquote() << "已为现有配置补齐默认抽屉:" << ConfigPath();
    }
}

QVector<Models::Drawer> LoadDrawers()
{
    const QJsonObject root = ReadConfigObject();

    QVector<Models::Drawer> drawers;
    int hotkeyIndex = 0;
    for (const auto &value : root.value(QStringLiteral("drawers")).toArray()) {
        const QJsonObject object = value.toObject();
        Models::Drawer drawer = DrawerFromJson(object);
        if (drawer.id.isEmpty() || drawer.name.isEmpty())
            continue;

        // 旧配置没有 hotkey 字段时, 按顺序补默认数字键
        if (!object.contains(QStringLiteral("hotkey")) && hotkeyIndex < 9)
            drawer.hotkey = QString::number(hotkeyIndex + 1);
        ++hotkeyIndex;
        drawers.append(drawer);
    }

    if (drawers.isEmpty())
        drawers = DefaultDrawers();

    return drawers;
}

int ManagerWindowWidth()
{
    return ReadSettingInt(QStringLiteral("managerWidth"), 0);
}

int ManagerWindowHeight()
{
    return ReadSettingInt(QStringLiteral("managerHeight"), 0);
}

void SetManagerWindowSize(int width, int height)
{
    WriteSettingInts({ qMakePair(QStringLiteral("managerWidth"), width),
                       qMakePair(QStringLiteral("managerHeight"), height) });
}

int FloatingWindowX()
{
    return ReadSettingInt(QStringLiteral("floatingX"), -1);
}

int FloatingWindowY()
{
    return ReadSettingInt(QStringLiteral("floatingY"), -1);
}

void SetFloatingPosition(int x, int y)
{
    WriteSettingInts({ qMakePair(QStringLiteral("floatingX"), x),
                       qMakePair(QStringLiteral("floatingY"), y) });
}

QString HotkeyAddWindow()
{
    return ReadHotkeyString(QStringLiteral("addWindow"), QStringLiteral("Ctrl+Alt+Z"));
}

QString HotkeyRemoveWindow()
{
    return ReadHotkeyString(QStringLiteral("removeWindow"), QStringLiteral("Ctrl+Alt+X"));
}

void SetWindowHotkeys(const QString &addWindow, const QString &removeWindow)
{
    QJsonObject root = ReadConfigObject();
    if (root.isEmpty())
        root = DefaultRoot();

    QJsonObject hotkeys = root.value(QStringLiteral("hotkeys")).toObject();
    // 清理已废弃的旧键位字段
    hotkeys.remove(QStringLiteral("switchPrefix"));
    hotkeys.remove(QStringLiteral("restoreAll"));
    hotkeys.remove(QStringLiteral("search"));
    hotkeys[QStringLiteral("addWindow")] = addWindow;
    hotkeys[QStringLiteral("removeWindow")] = removeWindow;
    root[QStringLiteral("hotkeys")] = hotkeys;
    WriteConfigObject(root);
}

bool SaveDrawers(const QVector<Models::Drawer> &drawers)
{
    QJsonObject root = ReadConfigObject();
    if (root.isEmpty())
        root = DefaultRoot();

    QJsonArray array;
    for (const auto &drawer : drawers)
        array.append(DrawerToJson(drawer));
    root[QStringLiteral("drawers")] = array;

    return WriteConfigObject(root);
}

}
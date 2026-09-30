// 文件用途: 抽屉服务实现, 处理窗口归属, 场景切换, 添加移除与恢复全部
#include "Services/DrawerService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include "Platform/ConfigStore.h"
#include "Platform/HotkeyService.h"
#include "Platform/Log.h"

namespace {

const QString kHotkeyPrefix = QStringLiteral("Ctrl+Alt");
constexpr int kMaxTodoLength = 200;

}

namespace DrawDesk::Services {

DrawerService::DrawerService(QObject *parent) : QObject(parent)
{
}

QStringList DrawerService::DrawerNames() const
{
    return m_drawerNames;
}

int DrawerService::ActiveIndex() const
{
    return m_activeIndex;
}

QString DrawerService::Notice() const
{
    return m_notice;
}

bool DrawerService::Load()
{
    m_drawers = ConfigStore::LoadDrawers();
    if (m_drawers.isEmpty()) {
        qWarning("抽屉配置为空, 服务未加载");
        return false;
    }

    m_activeIndex = 0;
    PruneStaleProcessIds();
    RefreshNames();

    emit drawersChanged();
    emit activeIndexChanged();

    qInfo().noquote() << "抽屉配置已加载, 数量:" << m_drawers.size();
    return true;
}

void DrawerService::SwitchTo(int index)
{
    if (index < 0 || index >= m_drawers.size() || index == m_activeIndex)
        return;
    if (m_restoring)
        return;

    PruneStaleProcessIds();

    // 离开前保存当前抽屉的窗口布局
    CaptureLayout(m_activeIndex);

    m_activeIndex = index;
    ApplyVisibility();
    RestoreLayout(index);

    if (m_persistChanges)
        ConfigStore::SaveDrawers(m_drawers);

    emit activeIndexChanged();

    const QString name = m_drawers.at(index).name;
    qInfo().noquote() << "切换到抽屉:" << name;
    Notify(QStringLiteral("已切换到 ") + name);
}

void DrawerService::AddForegroundWindowToDrawer(int index)
{
    if (index < 0 || index >= m_drawers.size())
        return;

    HWND handle = WindowApi::LastExternalForeground();
    if (!handle) {
        Notify(QStringLiteral("未找到目标窗口, 请先激活要加入的窗口"));
        return;
    }

    const WindowApi::WindowInfo info = WindowApi::DescribeWindow(handle);
    if (info.processName.isEmpty()) {
        Notify(QStringLiteral("无法识别该窗口"));
        return;
    }

    // 系统外壳窗口与工具窗口不参与抽屉管理
    if (!WindowApi::IsWindowCapturable(handle)) {
        Notify(QStringLiteral("该窗口不支持加入抽屉"));
        return;
    }

    // 同一窗口可以属于多个抽屉, 这里只检查当前抽屉是否已有匹配规则
    if (MatchesDrawer(info, index)) {
        Notify(QStringLiteral("该窗口已在 ") + m_drawers.at(index).name + QStringLiteral(" 中"));
        return;
    }

    Models::WindowRule rule;
    rule.process = info.processName;
    rule.titlePattern = info.title;
    rule.processId = static_cast<int>(info.processId);
    m_drawers[index].rules.append(rule);

    if (m_persistChanges)
        ConfigStore::SaveDrawers(m_drawers);

    emit drawersChanged();
    qInfo().noquote() << "窗口已加入抽屉:" << m_drawers.at(index).name << "窗口:" << info.processName
                      << info.title;
    Notify(QStringLiteral("已将 %1 加入 %2").arg(info.processName, m_drawers.at(index).name));

    ApplyVisibility();
}

void DrawerService::RemoveForegroundWindowFromDrawer(int index)
{
    if (index < 0 || index >= m_drawers.size())
        return;

    HWND handle = WindowApi::LastExternalForeground();
    if (!handle) {
        Notify(QStringLiteral("未找到目标窗口, 请先激活要移除的窗口"));
        return;
    }

    const WindowApi::WindowInfo info = WindowApi::DescribeWindow(handle);

    bool removed = false;
    auto &rules = m_drawers[index].rules;
    for (int i = 0; i < rules.size(); ++i) {
        if (WindowApi::MatchesRule(info, rules.at(i))) {
            rules.removeAt(i);
            removed = true;
            break;
        }
    }

    if (!removed) {
        Notify(QStringLiteral("此窗口不在 ") + m_drawers.at(index).name + QStringLiteral(" 中"));
        return;
    }

    if (m_persistChanges)
        ConfigStore::SaveDrawers(m_drawers);

    emit drawersChanged();
    qInfo().noquote() << "窗口已移出抽屉:" << m_drawers.at(index).name << "窗口:" << info.processName
                      << info.title;

    // 移除后重新计算可见性: 不再匹配任何抽屉时恢复显示, 仍匹配其他抽屉时保持隐藏
    ApplyVisibility();

    Notify(QStringLiteral("已从 %1 移除").arg(m_drawers.at(index).name));
}

void DrawerService::RestoreAll()
{
    if (m_restoring) {
        Notify(QStringLiteral("正在恢复窗口, 请稍候"));
        return;
    }
    m_restoring = true;

    int restored = 0;
    int processed = 0;

    const auto recorded = m_cloakedWindows;
    for (const quintptr key : recorded) {
        HWND handle = reinterpret_cast<HWND>(key);
        if (WindowApi::SetCloaked(handle, false))
            ++restored;
        m_cloakedWindows.remove(key);
        m_cloakedInfo.remove(key);

        // 分批处理, 给 DWM 和应用留出重绘时间
        if ((++processed % 8) == 0)
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    // 恢复规则覆盖但本次会话未记录的隐藏窗口, 应对上次异常退出
    const auto windows = WindowApi::EnumTopLevelWindows();
    for (const auto &info : windows) {
        if (DrawerIndexOf(info) >= 0 && WindowApi::IsCloaked(info.handle)) {
            if (WindowApi::SetCloaked(info.handle, false))
                ++restored;

            if ((++processed % 8) == 0)
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }
    }

    // 恢复状态文件里的隐藏窗口, 句柄优先, 进程与标题兜底
    restored += RestoreCloakedState();

    m_cloakedWindows.clear();
    m_cloakedInfo.clear();
    PersistCloakedState();

    // 等 DWM 把这一批窗口的合成变化处理完, 避免残影
    WindowApi::FlushComposition();
    m_restoring = false;

    qInfo().noquote() << "恢复全部窗口, 共恢复:" << restored;
    if (restored > 0)
        Notify(QStringLiteral("已恢复 %1 个窗口").arg(restored));
    else
        Notify(QStringLiteral("没有需要恢复的窗口"));
}
void DrawerService::QuitApplication()
{
    qInfo("已请求退出程序");
    if (QCoreApplication::instance())
        QMetaObject::invokeMethod(QCoreApplication::instance(), "quit", Qt::QueuedConnection);
}

void DrawerService::RemoveRule(int drawerIndex, int ruleIndex)
{
    if (drawerIndex < 0 || drawerIndex >= m_drawers.size())
        return;

    auto &rules = m_drawers[drawerIndex].rules;
    if (ruleIndex < 0 || ruleIndex >= rules.size())
        return;

    rules.removeAt(ruleIndex);

    if (m_persistChanges)
        ConfigStore::SaveDrawers(m_drawers);

    emit drawersChanged();

    // 规则变化后重新同步可见性: 不再被任何抽屉匹配的窗口恢复显示
    ApplyVisibility();
    Notify(QStringLiteral("已移除窗口条目"));
}

QVariantList DrawerService::DrawerPreview(int index) const
{
    QVariantList list;
    if (index < 0 || index >= m_drawers.size())
        return list;

    const auto windows = WindowApi::EnumTopLevelWindows();
    const auto &rules = m_drawers.at(index).rules;
    QSet<quintptr> assignedRects;
    for (int r = 0; r < rules.size(); ++r) {
        for (const auto &info : windows) {
            const quintptr key = reinterpret_cast<quintptr>(info.handle);
            if (assignedRects.contains(key))
                continue;
            if (!WindowApi::MatchesRule(info, rules.at(r)))
                continue;

            assignedRects.insert(key);
            const QRect rect = WindowApi::WindowRectOf(info.handle);
            QVariantMap item;
            item[QStringLiteral("ruleIndex")] = r;
            item[QStringLiteral("process")] = info.processName;
            item[QStringLiteral("title")] = info.title;
            item[QStringLiteral("x")] = rect.x();
            item[QStringLiteral("y")] = rect.y();
            item[QStringLiteral("w")] = rect.width();
            item[QStringLiteral("h")] = rect.height();
            list.append(item);
            break;
        }
    }
    return list;
}


QVariantList DrawerService::MonitorLayout() const
{
    QVariantList list;
    for (const auto &monitor : WindowApi::EnumMonitors()) {
        QVariantMap item;
        item[QStringLiteral("name")] = monitor.name;
        item[QStringLiteral("x")] = monitor.rect.x();
        item[QStringLiteral("y")] = monitor.rect.y();
        item[QStringLiteral("w")] = monitor.rect.width();
        item[QStringLiteral("h")] = monitor.rect.height();
        item[QStringLiteral("primary")] = monitor.primary;
        list.append(item);
    }
    return list;
}

void DrawerService::AddDrawer(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        Notify(QStringLiteral("抽屉名称不能为空"));
        return;
    }

    Models::Drawer drawer;
    drawer.id = QStringLiteral("drawer%1").arg(QDateTime::currentMSecsSinceEpoch());
    drawer.name = trimmed;
    m_drawers.append(drawer);

    RefreshNames();
    if (m_persistChanges)
        ConfigStore::SaveDrawers(m_drawers);

    emit drawersChanged();
    Notify(QStringLiteral("已添加抽屉 ") + trimmed);
}

void DrawerService::RenameDrawer(int index, const QString &name)
{
    if (index < 0 || index >= m_drawers.size())
        return;

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        Notify(QStringLiteral("抽屉名称不能为空"));
        return;
    }

    m_drawers[index].name = trimmed;
    RefreshNames();
    if (m_persistChanges)
        ConfigStore::SaveDrawers(m_drawers);

    emit drawersChanged();
    Notify(QStringLiteral("已重命名为 ") + trimmed);
}

void DrawerService::RemoveDrawer(int index)
{
    if (index < 0 || index >= m_drawers.size())
        return;

    const QString removedName = m_drawers.at(index).name;
    m_drawers.removeAt(index);

    if (m_activeIndex > index)
        --m_activeIndex;
    if (m_activeIndex >= m_drawers.size())
        m_activeIndex = m_drawers.size() - 1;
    if (m_activeIndex < 0)
        m_activeIndex = 0;

    RefreshNames();
    if (m_persistChanges)
        ConfigStore::SaveDrawers(m_drawers);

    emit drawersChanged();
    emit activeIndexChanged();

    // 仅属于该抽屉的窗口恢复显示, 仍匹配其他抽屉的窗口保持隐藏
    ApplyVisibility();
    Notify(QStringLiteral("已删除抽屉 ") + removedName);
}


void DrawerService::MoveDrawerUp(int index)
{
    if (index <= 0 || index >= m_drawers.size())
        return;

    m_drawers.swapItemsAt(index, index - 1);

    if (m_activeIndex == index)
        m_activeIndex = index - 1;
    else if (m_activeIndex == index - 1)
        m_activeIndex = index;

    RefreshNames();
    if (m_persistChanges)
        ConfigStore::SaveDrawers(m_drawers);

    emit drawersChanged();
    emit activeIndexChanged();
    Notify(QStringLiteral("已调整抽屉顺序"));
}

void DrawerService::MoveDrawerDown(int index)
{
    if (index < 0 || index >= m_drawers.size() - 1)
        return;

    m_drawers.swapItemsAt(index, index + 1);

    if (m_activeIndex == index)
        m_activeIndex = index + 1;
    else if (m_activeIndex == index + 1)
        m_activeIndex = index;

    RefreshNames();
    if (m_persistChanges)
        ConfigStore::SaveDrawers(m_drawers);

    emit drawersChanged();
    emit activeIndexChanged();
    Notify(QStringLiteral("已调整抽屉顺序"));
}

QString DrawerService::RuleProcess(int drawerIndex, int ruleIndex) const
{
    if (drawerIndex < 0 || drawerIndex >= m_drawers.size())
        return QString();

    const auto &rules = m_drawers.at(drawerIndex).rules;
    if (ruleIndex < 0 || ruleIndex >= rules.size())
        return QString();

    return rules.at(ruleIndex).process;
}

QString DrawerService::RuleTitle(int drawerIndex, int ruleIndex) const
{
    if (drawerIndex < 0 || drawerIndex >= m_drawers.size())
        return QString();

    const auto &rules = m_drawers.at(drawerIndex).rules;
    if (ruleIndex < 0 || ruleIndex >= rules.size())
        return QString();

    return rules.at(ruleIndex).titlePattern;
}

int DrawerService::RuleProcessId(int drawerIndex, int ruleIndex) const
{
    if (drawerIndex < 0 || drawerIndex >= m_drawers.size())
        return 0;

    const auto &rules = m_drawers.at(drawerIndex).rules;
    if (ruleIndex < 0 || ruleIndex >= rules.size())
        return 0;

    return rules.at(ruleIndex).processId;
}

bool DrawerService::RuleUseRegex(int drawerIndex, int ruleIndex) const
{
    if (drawerIndex < 0 || drawerIndex >= m_drawers.size())
        return false;

    const auto &rules = m_drawers.at(drawerIndex).rules;
    if (ruleIndex < 0 || ruleIndex >= rules.size())
        return false;

    return rules.at(ruleIndex).useRegex;
}

qulonglong DrawerService::LiveProcessIdForRule(int drawerIndex, int ruleIndex) const
{
    if (drawerIndex < 0 || drawerIndex >= m_drawers.size())
        return 0;

    const auto &rules = m_drawers.at(drawerIndex).rules;
    if (ruleIndex < 0 || ruleIndex >= rules.size())
        return 0;

    const Models::WindowRule &rule = rules.at(ruleIndex);
    const auto windows = WindowApi::EnumTopLevelWindows();
    for (const auto &info : windows) {
        if (WindowApi::MatchesRule(info, rule))
            return info.processId;
    }
    return 0;
}

bool DrawerService::UpdateRule(int drawerIndex, int ruleIndex, const QString &process,
                               const QString &titlePattern, int processId, bool useRegex)
{
    if (drawerIndex < 0 || drawerIndex >= m_drawers.size())
        return false;

    auto &rules = m_drawers[drawerIndex].rules;
    if (ruleIndex < 0 || ruleIndex >= rules.size())
        return false;

    const QString trimmedProcess = process.trimmed();
    if (trimmedProcess.isEmpty()) {
        Notify(QStringLiteral("进程名不能为空"));
        return false;
    }

    rules[ruleIndex].process = trimmedProcess;
    rules[ruleIndex].titlePattern = titlePattern.trimmed();
    rules[ruleIndex].processId = processId > 0 ? processId : 0;
    rules[ruleIndex].useRegex = useRegex;

    if (m_persistChanges)
        ConfigStore::SaveDrawers(m_drawers);

    emit drawersChanged();
    Notify(QStringLiteral("规则已更新"));
    ApplyVisibility();
    return true;
}

QVariantList DrawerService::TodoItems(int drawerIndex) const
{
    QVariantList list;
    if (drawerIndex < 0 || drawerIndex >= m_drawers.size())
        return list;

    const auto &todos = m_drawers.at(drawerIndex).todos;
    auto append = [&list](int index, const Models::TodoItem &todo) {
        QVariantMap item;
        item[QStringLiteral("index")] = index;
        item[QStringLiteral("text")] = todo.text;
        item[QStringLiteral("done")] = todo.done;
        list.append(item);
    };

    // 未完成在前, 已完成在后
    for (int i = 0; i < todos.size(); ++i) {
        if (!todos.at(i).done)
            append(i, todos.at(i));
    }
    for (int i = 0; i < todos.size(); ++i) {
        if (todos.at(i).done)
            append(i, todos.at(i));
    }
    return list;
}

int DrawerService::PendingTodoCount(int drawerIndex) const
{
    if (drawerIndex < 0 || drawerIndex >= m_drawers.size())
        return 0;

    int count = 0;
    for (const auto &todo : m_drawers.at(drawerIndex).todos) {
        if (!todo.done)
            ++count;
    }
    return count;
}

bool DrawerService::AddTodo(int drawerIndex, const QString &text)
{
    if (drawerIndex < 0 || drawerIndex >= m_drawers.size())
        return false;

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        Notify(QStringLiteral("待办内容不能为空"));
        return false;
    }
    if (trimmed.size() > kMaxTodoLength) {
        Notify(QStringLiteral("待办内容不能超过 %1 个字符").arg(kMaxTodoLength));
        return false;
    }

    Models::TodoItem todo;
    todo.text = trimmed;
    m_drawers[drawerIndex].todos.append(todo);

    if (m_persistChanges)
        ConfigStore::SaveDrawers(m_drawers);
    emit todosChanged();
    return true;
}

bool DrawerService::SetTodoDone(int drawerIndex, int todoIndex, bool done)
{
    if (drawerIndex < 0 || drawerIndex >= m_drawers.size())
        return false;

    auto &todos = m_drawers[drawerIndex].todos;
    if (todoIndex < 0 || todoIndex >= todos.size())
        return false;
    if (todos.at(todoIndex).done == done)
        return true;

    todos[todoIndex].done = done;
    if (m_persistChanges)
        ConfigStore::SaveDrawers(m_drawers);
    emit todosChanged();
    return true;
}

bool DrawerService::UpdateTodoText(int drawerIndex, int todoIndex, const QString &text)
{
    if (drawerIndex < 0 || drawerIndex >= m_drawers.size())
        return false;

    auto &todos = m_drawers[drawerIndex].todos;
    if (todoIndex < 0 || todoIndex >= todos.size())
        return false;

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        Notify(QStringLiteral("待办内容不能为空"));
        return false;
    }
    if (trimmed.size() > kMaxTodoLength) {
        Notify(QStringLiteral("待办内容不能超过 %1 个字符").arg(kMaxTodoLength));
        return false;
    }
    if (todos.at(todoIndex).text == trimmed)
        return true;

    todos[todoIndex].text = trimmed;
    if (m_persistChanges)
        ConfigStore::SaveDrawers(m_drawers);
    emit todosChanged();
    return true;
}

bool DrawerService::RemoveTodo(int drawerIndex, int todoIndex)
{
    if (drawerIndex < 0 || drawerIndex >= m_drawers.size())
        return false;

    auto &todos = m_drawers[drawerIndex].todos;
    if (todoIndex < 0 || todoIndex >= todos.size())
        return false;

    todos.removeAt(todoIndex);
    if (m_persistChanges)
        ConfigStore::SaveDrawers(m_drawers);
    emit todosChanged();
    return true;
}

int DrawerService::ClearCompletedTodos(int drawerIndex)
{
    if (drawerIndex < 0 || drawerIndex >= m_drawers.size())
        return 0;

    auto &todos = m_drawers[drawerIndex].todos;
    int removed = 0;
    for (int i = todos.size() - 1; i >= 0; --i) {
        if (todos.at(i).done) {
            todos.removeAt(i);
            ++removed;
        }
    }

    if (removed > 0) {
        if (m_persistChanges)
            ConfigStore::SaveDrawers(m_drawers);
        emit todosChanged();
        Notify(QStringLiteral("已清除 %1 条已完成待办").arg(removed));
    }
    return removed;
}

QStringList DrawerService::SearchWindows(const QString &keyword)
{
    m_searchResults.clear();

    const QString key = keyword.trimmed();
    if (key.isEmpty())
        return QStringList();

    const auto windows = WindowApi::EnumTopLevelWindows();
    for (const auto &info : windows) {
        if (!WindowApi::IsWindowManageable(info.handle, true))
            continue;

        const bool matched = info.title.contains(key, Qt::CaseInsensitive)
                             || info.processName.contains(key, Qt::CaseInsensitive);
        if (!matched)
            continue;

        m_searchResults.append(info);
        if (m_searchResults.size() >= 6)
            break;
    }

    QStringList display;
    for (const auto &info : m_searchResults) {
        const QString title = info.title.isEmpty() ? QStringLiteral("(无标题)") : info.title;
        display.append(QStringLiteral("%1  |  %2").arg(info.processName, title));
    }
    return display;
}

bool DrawerService::AddSearchResultToDrawer(int resultIndex, int drawerIndex)
{
    if (resultIndex < 0 || resultIndex >= m_searchResults.size())
        return false;
    if (drawerIndex < 0 || drawerIndex >= m_drawers.size())
        return false;

    const WindowApi::WindowInfo info = m_searchResults.at(resultIndex);
    if (!IsWindow(info.handle)) {
        Notify(QStringLiteral("窗口已关闭"));
        return false;
    }

    if (!WindowApi::IsWindowManageable(info.handle, true)) {
        Notify(QStringLiteral("该窗口不支持加入抽屉"));
        return false;
    }

    if (MatchesDrawer(info, drawerIndex)) {
        Notify(QStringLiteral("该窗口已在 ") + m_drawers.at(drawerIndex).name + QStringLiteral(" 中"));
        return false;
    }

    Models::WindowRule rule;
    rule.process = info.processName;
    rule.titlePattern = info.title;
    rule.processId = static_cast<int>(info.processId);
    m_drawers[drawerIndex].rules.append(rule);

    if (m_persistChanges)
        ConfigStore::SaveDrawers(m_drawers);

    emit drawersChanged();
    Notify(QStringLiteral("已将 %1 加入 %2").arg(info.processName, m_drawers.at(drawerIndex).name));
    ApplyVisibility();
    return true;
}

void DrawerService::ActivateSearchResult(int resultIndex)
{
    if (resultIndex < 0 || resultIndex >= m_searchResults.size())
        return;

    const WindowApi::WindowInfo info = m_searchResults.at(resultIndex);
    if (!IsWindow(info.handle)) {
        Notify(QStringLiteral("窗口已关闭"));
        return;
    }

    // 窗口已匹配当前抽屉时不用切换, 否则切到第一个匹配的抽屉
    if (!MatchesDrawer(info, m_activeIndex)) {
        const int owner = DrawerIndexOf(info);
        if (owner >= 0 && owner != m_activeIndex)
            SwitchTo(owner);
    }

    WindowApi::UncloakAndActivate(info.handle);
    Notify(QStringLiteral("已切换到窗口"));
}


void DrawerService::OpenConfigFolder()
{
    ConfigStore::EnsureDefaultConfig();
    WindowApi::ShowInExplorer(ConfigStore::ConfigPath());
}

QString DrawerService::HotkeyAddWindow()
{
    return ConfigStore::HotkeyAddWindow();
}

QString DrawerService::HotkeyRemoveWindow()
{
    return ConfigStore::HotkeyRemoveWindow();
}

QString DrawerService::DrawerHotkey(int index) const
{
    if (index < 0 || index >= m_drawers.size())
        return QString();

    return m_drawers.at(index).hotkey;
}

bool DrawerService::SaveDrawerHotkey(int index, const QString &key)
{
    if (index < 0 || index >= m_drawers.size())
        return false;

    const QString trimmed = key.trimmed();
    const QString prefix = kHotkeyPrefix;
    const QString drawerName = m_drawers.at(index).name;

    if (trimmed.isEmpty()) {
        m_drawers[index].hotkey.clear();
        if (m_persistChanges)
            ConfigStore::SaveDrawers(m_drawers);
        emit hotkeySettingsChanged();
        Notify(QStringLiteral("已取消 %1 的快捷键").arg(drawerName));
        return true;
    }

    UINT modifiers = 0;
    UINT virtualKey = 0;
    if (!HotkeyService::ParseHotkey(prefix + QLatin1Char('+') + trimmed, &modifiers,
                                     &virtualKey)) {
        Notify(QStringLiteral("键位无效, 请输入单个数字, 字母或符号"));
        return false;
    }

    // 检查与全局加入, 移出以及其他抽屉的冲突
    UINT otherModifiers = 0;
    UINT otherKey = 0;
    if (HotkeyService::ParseHotkey(ConfigStore::HotkeyAddWindow(), &otherModifiers, &otherKey)
        && otherModifiers == modifiers && otherKey == virtualKey) {
        Notify(QStringLiteral("与加入窗口快捷键重复"));
        return false;
    }
    if (HotkeyService::ParseHotkey(ConfigStore::HotkeyRemoveWindow(), &otherModifiers,
                                  &otherKey)
        && otherModifiers == modifiers && otherKey == virtualKey) {
        Notify(QStringLiteral("与移出窗口快捷键重复"));
        return false;
    }
    for (int i = 0; i < m_drawers.size(); ++i) {
        if (i == index || m_drawers.at(i).hotkey.isEmpty())
            continue;
        UINT mods = 0;
        UINT vk = 0;
        if (HotkeyService::ParseHotkey(prefix + QLatin1Char('+') + m_drawers.at(i).hotkey,
                                       &mods, &vk)
            && mods == modifiers && vk == virtualKey) {
            Notify(QStringLiteral("与 %1 的快捷键重复").arg(m_drawers.at(i).name));
            return false;
        }
    }

    m_drawers[index].hotkey = trimmed;
    if (m_persistChanges)
        ConfigStore::SaveDrawers(m_drawers);
    emit hotkeySettingsChanged();

    if (HotkeyService::IsRegistered(100 + index))
        Notify(QStringLiteral("快捷键已更新"));
    else
        Notify(QStringLiteral("快捷键已保存, 但被其他程序占用, 请更换"));
    return true;
}

bool DrawerService::SaveWindowHotkeys(const QString &addWindow, const QString &removeWindow)
{
    UINT addModifiers = 0;
    UINT addKey = 0;
    if (!HotkeyService::ParseHotkey(addWindow.trimmed(), &addModifiers, &addKey)) {
        Notify(QStringLiteral("加入窗口快捷键无效, 示例: Ctrl+Alt+Z"));
        return false;
    }

    UINT removeModifiers = 0;
    UINT removeKey = 0;
    if (!HotkeyService::ParseHotkey(removeWindow.trimmed(), &removeModifiers, &removeKey)) {
        Notify(QStringLiteral("移出窗口快捷键无效, 示例: Ctrl+Alt+X"));
        return false;
    }

    if (addModifiers == removeModifiers && addKey == removeKey) {
        Notify(QStringLiteral("加入与移出快捷键不能相同"));
        return false;
    }

    // 抽屉自身的键位不能与新保存的全局快捷键重复
    for (const auto &drawer : m_drawers) {
        if (drawer.hotkey.isEmpty())
            continue;

        UINT drawerModifiers = 0;
        UINT drawerKey = 0;
        if (!HotkeyService::ParseHotkey(kHotkeyPrefix + QLatin1Char('+') + drawer.hotkey,
                                         &drawerModifiers, &drawerKey))
            continue;

        const bool duplicate = (drawerModifiers == addModifiers && drawerKey == addKey)
                               || (drawerModifiers == removeModifiers && drawerKey == removeKey);
        if (duplicate) {
            Notify(QStringLiteral("与 %1 的快捷键重复").arg(drawer.name));
            return false;
        }
    }

    ConfigStore::SetWindowHotkeys(addWindow.trimmed(), removeWindow.trimmed());
    emit hotkeySettingsChanged();

    // 信号为同步连接, 此时已完成重新注册, 可直接检查注册结果
    bool allRegistered = HotkeyService::IsRegistered(0) && HotkeyService::IsRegistered(1);
    for (int i = 0; i < m_drawers.size() && allRegistered; ++i) {
        if (!m_drawers.at(i).hotkey.isEmpty())
            allRegistered = HotkeyService::IsRegistered(100 + i);
    }

    if (allRegistered)
        Notify(QStringLiteral("快捷键已更新"));
    else
        Notify(QStringLiteral("快捷键已保存, 部分组合被其他程序占用, 请更换"));
    return true;
}
int DrawerService::CaptureDesktopToDrawer(int index)
{
    if (index < 0 || index >= m_drawers.size())
        return 0;

    const auto windows = WindowApi::EnumTopLevelWindows();
    int added = 0;
    for (const auto &info : windows) {
        if (added >= 40)
            break;

        if (!WindowApi::IsWindowCapturable(info.handle))
            continue;

        // 同一窗口可以加入多个抽屉, 当前抽屉已有匹配则跳过
        if (MatchesDrawer(info, index))
            continue;

        Models::WindowRule rule;
        rule.process = info.processName;
        rule.titlePattern = info.title;
        rule.processId = static_cast<int>(info.processId);
        m_drawers[index].rules.append(rule);
        ++added;
    }

    if (added > 0) {
        if (m_persistChanges)
            ConfigStore::SaveDrawers(m_drawers);

        // 将选中抽屉切为活动, 保证被抓取的窗口保持可见
        if (index != m_activeIndex)
            SwitchTo(index);
        else
            ApplyVisibility();

        emit drawersChanged();
        Notify(QStringLiteral("已抓取 %1 个窗口到 %2 抽屉")
                   .arg(added)
                   .arg(m_drawers.at(index).name));
    } else {
        Notify(QStringLiteral("未发现可抓取的窗口"));
    }
    return added;
}

int DrawerService::ManagerWindowWidth()
{
    return ConfigStore::ManagerWindowWidth();
}

int DrawerService::ManagerWindowHeight()
{
    return ConfigStore::ManagerWindowHeight();
}

void DrawerService::SaveManagerWindowSize(int width, int height)
{
    ConfigStore::SetManagerWindowSize(width, height);
}

int DrawerService::FloatingWindowX()
{
    return ConfigStore::FloatingWindowX();
}

int DrawerService::FloatingWindowY()
{
    return ConfigStore::FloatingWindowY();
}

void DrawerService::SaveFloatingPosition(int x, int y)
{
    ConfigStore::SetFloatingPosition(x, y);
}

QStringList DrawerService::DrawerWindowDetails(int index) const
{
    QStringList items;
    if (index < 0 || index >= m_drawers.size())
        return items;

    const auto windows = WindowApi::EnumTopLevelWindows();
    QSet<quintptr> assigned;
    for (const auto &rule : m_drawers.at(index).rules) {
        int matchIndex = -1;
        for (int i = 0; i < windows.size(); ++i) {
            const quintptr key = reinterpret_cast<quintptr>(windows.at(i).handle);
            if (assigned.contains(key))
                continue;
            if (WindowApi::MatchesRule(windows.at(i), rule)) {
                matchIndex = i;
                break;
            }
        }

        const QString title = rule.titlePattern.isEmpty() ? QStringLiteral("(任意标题)")
                                                          : rule.titlePattern;

        QString header;
        QString detail;
        if (matchIndex >= 0) {
            const WindowApi::WindowInfo &info = windows.at(matchIndex);
            assigned.insert(reinterpret_cast<quintptr>(info.handle));

            const QString handleText =
                QString::number(reinterpret_cast<quintptr>(info.handle), 16).toUpper();
            header = QStringLiteral("● %1 (PID %2, 窗口 %3) | %4")
                         .arg(info.processName)
                         .arg(info.processId)
                         .arg(handleText, title);
            const WindowApi::WindowPlacement placement = WindowApi::GetPlacement(info.handle);
            const QString monitor = WindowApi::MonitorNameOf(info.handle);

            QString state = QStringLiteral("正常");
            if (IsZoomed(info.handle))
                state = QStringLiteral("最大化");
            else if (IsIconic(info.handle))
                state = QStringLiteral("最小化");

            detail = QStringLiteral("    %1 · %2,%3 · %4x%5 · %6")
                         .arg(monitor)
                         .arg(placement.normalRect.x())
                         .arg(placement.normalRect.y())
                         .arg(placement.normalRect.width())
                         .arg(placement.normalRect.height())
                         .arg(state);
        } else {
            header = QStringLiteral("○ %1 | %2").arg(rule.process, title);
            detail = QStringLiteral("    未打开");
        }

        items.append(header + QStringLiteral("\n") + detail);
    }
    return items;
}

void DrawerService::SetDrawersForTest(const QVector<Models::Drawer> &drawers)
{
    m_drawers = drawers;
    m_activeIndex = 0;
    m_persistChanges = false;
    RefreshNames();

    emit drawersChanged();
    emit activeIndexChanged();
}

bool DrawerService::MatchesDrawer(const WindowApi::WindowInfo &info, int index) const
{
    if (index < 0 || index >= m_drawers.size())
        return false;

    for (const auto &rule : m_drawers.at(index).rules) {
        if (WindowApi::MatchesRule(info, rule))
            return true;
    }
    return false;
}

int DrawerService::DrawerIndexOf(const WindowApi::WindowInfo &info) const
{
    // 完全按各抽屉自己的规则匹配, 同一窗口可以同时匹配多个抽屉
    for (int i = 0; i < m_drawers.size(); ++i) {
        if (MatchesDrawer(info, i))
            return i;
    }
    return -1;
}

void DrawerService::CaptureLayout(int index)
{
    if (index < 0 || index >= m_drawers.size())
        return;

    auto &layout = m_drawers[index].layout;
    layout.clear();

    const auto windows = WindowApi::EnumTopLevelWindows();
    for (const auto &info : windows) {
        if (!MatchesDrawer(info, index))
            continue;

        const WindowApi::WindowPlacement placement = WindowApi::GetPlacement(info.handle);
        if (!placement.valid)
            continue;

        Models::WindowLayout item;
        item.process = info.processName;
        item.titlePattern = info.title;
        item.processId = static_cast<int>(info.processId);
        item.rect = placement.normalRect;
        item.showCommand = placement.showCommand;
        for (const auto &rule : m_drawers.at(index).rules) {
            if (WindowApi::MatchesRule(info, rule)) {
                item.useRegex = rule.useRegex;
                break;
            }
        }
        layout.append(item);
    }

    qInfo().noquote() << "已保存抽屉布局:" << m_drawers.at(index).name
                      << "窗口数:" << layout.size();
}

void DrawerService::RestoreLayout(int index)
{
    if (index < 0 || index >= m_drawers.size())
        return;

    const auto &layout = m_drawers[index].layout;
    if (layout.isEmpty())
        return;

    QVector<bool> used(layout.size(), false);
    const auto windows = WindowApi::EnumTopLevelWindows();
    for (const auto &info : windows) {
        if (!MatchesDrawer(info, index))
            continue;

        // 每个布局条目只应用到一个窗口, 避免同名窗口互相串位
        int layoutIndex = -1;
        for (int i = 0; i < layout.size(); ++i) {
            if (used.at(i))
                continue;

            const Models::WindowLayout &item = layout.at(i);
            Models::WindowRule rule;
            rule.process = item.process;
            rule.titlePattern = item.titlePattern;
            rule.processId = item.processId;
            rule.useRegex = item.useRegex;
            if (WindowApi::MatchesRule(info, rule)) {
                layoutIndex = i;
                break;
            }
        }
        if (layoutIndex < 0)
            continue;

        used[layoutIndex] = true;
        const Models::WindowLayout &item = layout.at(layoutIndex);

        WindowApi::WindowPlacement placement;
        placement.valid = true;
        placement.normalRect = item.rect;
        placement.showCommand = item.showCommand;

        // 位置不在任何显示器内时, 移动到主显示器工作区
        if (!WindowApi::IsRectOnAnyMonitor(item.rect)) {
            const QRect work = WindowApi::PrimaryWorkArea();
            qWarning().noquote() << "布局位置超出显示器范围, 移动到主屏:" << info.title;
            placement.normalRect.setTopLeft(QPoint(work.left() + 60, work.top() + 60));
        }

        if (!WindowApi::ApplyPlacement(info.handle, placement)) {
            qWarning().noquote() << "应用窗口布局失败:" << info.processName << info.title;
        }
    }
}
void DrawerService::ApplyVisibility()
{
    int matched = 0;
    int hidden = 0;
    int shown = 0;
    int released = 0;

    const auto windows = WindowApi::EnumTopLevelWindows();
    for (const auto &info : windows) {
        const bool inActive = MatchesDrawer(info, m_activeIndex);
        if (inActive) {
            // 活动抽屉优先: 同一窗口也匹配其他抽屉时保持显示
            ++matched;
            if (UncloakWindow(info.handle))
                ++shown;
            continue;
        }

        if (DrawerIndexOf(info) >= 0) {
            ++matched;
            if (CloakWindow(info.handle))
                ++hidden;
            continue;
        }

        // 规则删除或调整后不再属于任何抽屉时, 恢复本程序隐藏的窗口
        if (m_cloakedWindows.contains(reinterpret_cast<quintptr>(info.handle))
            && UncloakWindow(info.handle)) {
            ++released;
        }
    }

    const QString activeName = (m_activeIndex >= 0 && m_activeIndex < m_drawers.size())
                                   ? m_drawers.at(m_activeIndex).name
                                   : QStringLiteral("无");
    qInfo().noquote() << "可见性调整: 受管窗口" << matched << "个, 隐藏" << hidden << "个, 显示"
                      << shown << "个, 恢复" << released << "个, 活动抽屉:" << activeName;

    PersistCloakedState();
    if (hidden + shown + released > 0)
        WindowApi::FlushComposition();
}

bool DrawerService::CloakWindow(HWND handle)
{
    // 已被 DWM 隐藏或已不可见时不再重复隐藏
    if (WindowApi::IsCloaked(handle) || !IsWindowVisible(handle))
        return false;

    if (!WindowApi::SetCloaked(handle, true)) {
        qWarning().noquote() << "隐藏窗口失败, 句柄:" << reinterpret_cast<quintptr>(handle);
        return false;
    }

    const quintptr key = reinterpret_cast<quintptr>(handle);
    m_cloakedWindows.insert(key);
    m_cloakedInfo.insert(key, WindowApi::DescribeWindow(handle));
    return true;
}

bool DrawerService::UncloakWindow(HWND handle)
{
    const quintptr key = reinterpret_cast<quintptr>(handle);
    if (!m_cloakedWindows.contains(key))
        return false;

    if (!WindowApi::SetCloaked(handle, false)) {
        qWarning().noquote() << "恢复窗口失败, 句柄:" << key;
        return false;
    }

    m_cloakedWindows.remove(key);
    m_cloakedInfo.remove(key);
    return true;
}

bool DrawerService::IsProcessAlive(int processId, const QString &processName) const
{
    if (processId <= 0)
        return false;

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                 static_cast<DWORD>(processId));
    if (!process)
        return false;
    CloseHandle(process);

    if (processName.isEmpty())
        return true;

    // 进程号可能被系统复用, 名称也要一致
    return QString::compare(WindowApi::ProcessNameOf(static_cast<DWORD>(processId)), processName,
                            Qt::CaseInsensitive)
           == 0;
}

void DrawerService::PruneStaleProcessIds()
{
    bool changed = false;
    for (auto &drawer : m_drawers) {
        for (auto &rule : drawer.rules) {
            if (rule.processId != 0 && !IsProcessAlive(rule.processId, rule.process)) {
                rule.processId = 0;
                changed = true;
            }
        }
        for (auto &item : drawer.layout) {
            if (item.processId != 0 && !IsProcessAlive(item.processId, item.process)) {
                item.processId = 0;
                changed = true;
            }
        }
    }

    if (changed) {
        qInfo().noquote() << "已清理失效的进程号";
        if (m_persistChanges)
            ConfigStore::SaveDrawers(m_drawers);
    }
}

void DrawerService::PersistCloakedState()
{
    if (!m_persistChanges)
        return;

    const QString path = ConfigStore::CloakedStatePath();
    if (m_cloakedInfo.isEmpty()) {
        QFile::remove(path);
        return;
    }

    QJsonArray windows;
    for (auto it = m_cloakedInfo.constBegin(); it != m_cloakedInfo.constEnd(); ++it) {
        const WindowApi::WindowInfo &info = it.value();
        QJsonObject object;
        object[QStringLiteral("handle")] = QString::number(it.key(), 16);
        object[QStringLiteral("processId")] = static_cast<int>(info.processId);
        object[QStringLiteral("process")] = info.processName;
        object[QStringLiteral("title")] = info.title;
        windows.append(object);
    }

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("updatedAt")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root[QStringLiteral("windows")] = windows;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning().noquote() << "隐藏状态写入失败:" << file.errorString();
        return;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    if (!file.commit())
        qWarning().noquote() << "隐藏状态提交失败:" << file.errorString();
}

int DrawerService::RestoreCloakedState()
{
    const QString path = ConfigStore::CloakedStatePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return 0;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!document.isObject())
        return 0;

    const QJsonArray entries = document.object().value(QStringLiteral("windows")).toArray();
    if (entries.isEmpty()) {
        QFile::remove(path);
        return 0;
    }

    const auto windows = WindowApi::EnumTopLevelWindows();
    int restored = 0;
    for (const auto &entryValue : entries) {
        const QJsonObject entry = entryValue.toObject();
        const QString process = entry.value(QStringLiteral("process")).toString();
        const QString title = entry.value(QStringLiteral("title")).toString();
        const DWORD processId = static_cast<DWORD>(entry.value(QStringLiteral("processId")).toInt());

        bool handleValid = false;
        const quintptr rawHandle =
            entry.value(QStringLiteral("handle")).toString().toULongLong(&handleValid, 16);
        bool done = false;

        // 优先按句柄恢复, 标题变化也不受影响
        if (handleValid && rawHandle != 0) {
            HWND handle = reinterpret_cast<HWND>(rawHandle);
            if (IsWindow(handle)) {
                const WindowApi::WindowInfo info = WindowApi::DescribeWindow(handle);
                const bool processMatches =
                    (processId == 0 || info.processId == processId)
                    && (process.isEmpty()
                        || QString::compare(info.processName, process, Qt::CaseInsensitive) == 0);
                if (processMatches) {
                    WindowApi::SetCloaked(handle, false);
                    if (!IsWindowVisible(handle))
                        ShowWindow(handle, SW_SHOWNA);
                    ++restored;
                    done = true;
                }
            }
        }

        // 句柄失效时按进程与标题查找
        if (!done) {
            for (const auto &info : windows) {
                if (processId != 0 && info.processId != processId)
                    continue;
                if (!process.isEmpty()
                    && QString::compare(info.processName, process, Qt::CaseInsensitive) != 0)
                    continue;
                if (!title.isEmpty() && info.title != title)
                    continue;

                WindowApi::SetCloaked(info.handle, false);
                if (!IsWindowVisible(info.handle))
                    ShowWindow(info.handle, SW_SHOWNA);
                ++restored;
                break;
            }
        }
    }

    QFile::remove(path);
    qInfo().noquote() << "状态文件恢复窗口:" << restored;
    return restored;
}

void DrawerService::RefreshNames()
{
    m_drawerNames.clear();
    for (const auto &drawer : m_drawers)
        m_drawerNames.append(drawer.name);
}

void DrawerService::Notify(const QString &text)
{
    qInfo().noquote() << "提示:" << text;

    // 界面内提示, 不使用系统通知与提示音
    m_notice = text;
    emit noticeChanged();
}

}
// 文件用途: 抽屉服务, 管理抽屉配置与窗口切换
#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVector>

#include "Models/Drawer.h"
#include "Platform/WindowApi.h"

namespace DrawDesk::Services {

// 抽屉服务, 负责窗口归属判断, 场景切换与恢复
class DrawerService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList drawerNames READ DrawerNames NOTIFY drawersChanged)
    Q_PROPERTY(int activeIndex READ ActiveIndex NOTIFY activeIndexChanged)
    Q_PROPERTY(QString notice READ Notice NOTIFY noticeChanged)

public:
    explicit DrawerService(QObject *parent = nullptr);

    QStringList DrawerNames() const;
    int ActiveIndex() const;
    QString Notice() const;

    // 从配置加载抽屉列表, 返回是否成功
    bool Load();

    // 切换到指定抽屉, 隐藏其他抽屉的窗口
    Q_INVOKABLE void SwitchTo(int index);

    // 把最近的外部前台窗口加入指定抽屉
    Q_INVOKABLE void AddForegroundWindowToDrawer(int index);

    // 把最近的外部前台窗口从指定抽屉移除
    Q_INVOKABLE void RemoveForegroundWindowFromDrawer(int index);

    // 恢复全部由本程序隐藏的窗口以及规则覆盖的隐藏窗口
    Q_INVOKABLE void RestoreAll();

    // 请求程序退出, 由 C++ 排队处理, 避免在关闭事件里同步退出
    Q_INVOKABLE void QuitApplication();

    // 从抽屉移除指定序号的窗口条目
    Q_INVOKABLE void RemoveRule(int drawerIndex, int ruleIndex);

    // 返回抽屉内窗口的详情, 每项为两行文本(窗口与位置)
    Q_INVOKABLE QStringList DrawerWindowDetails(int index) const;

    // 返回窗口矩形列表, 用于图形化预览
    Q_INVOKABLE QVariantList DrawerPreview(int index) const;

    // 返回显示器布局列表
    Q_INVOKABLE QVariantList MonitorLayout() const;


    // 抽屉管理: 添加, 重命名, 删除, 调整顺序
    Q_INVOKABLE void AddDrawer(const QString &name);
    Q_INVOKABLE void RenameDrawer(int index, const QString &name);
    Q_INVOKABLE void RemoveDrawer(int index);
    Q_INVOKABLE void MoveDrawerUp(int index);
    Q_INVOKABLE void MoveDrawerDown(int index);

    // 规则编辑
    Q_INVOKABLE QString RuleProcess(int drawerIndex, int ruleIndex) const;
    Q_INVOKABLE QString RuleTitle(int drawerIndex, int ruleIndex) const;
    Q_INVOKABLE int RuleProcessId(int drawerIndex, int ruleIndex) const;
    Q_INVOKABLE bool RuleUseRegex(int drawerIndex, int ruleIndex) const;

    // 返回规则当前匹配窗口的 PID, 未匹配返回 0
    Q_INVOKABLE qulonglong LiveProcessIdForRule(int drawerIndex, int ruleIndex) const;

    Q_INVOKABLE bool UpdateRule(int drawerIndex, int ruleIndex, const QString &process,
                                const QString &titlePattern, int processId, bool useRegex);

    // 抽屉待办: 列表按未完成在前, 已完成在后返回
    Q_INVOKABLE QVariantList TodoItems(int drawerIndex) const;
    Q_INVOKABLE int PendingTodoCount(int drawerIndex) const;
    Q_INVOKABLE bool AddTodo(int drawerIndex, const QString &text);
    Q_INVOKABLE bool SetTodoDone(int drawerIndex, int todoIndex, bool done);
    Q_INVOKABLE bool UpdateTodoText(int drawerIndex, int todoIndex, const QString &text);
    Q_INVOKABLE bool RemoveTodo(int drawerIndex, int todoIndex);
    Q_INVOKABLE int ClearCompletedTodos(int drawerIndex);

    // 搜索窗口, 返回用于显示的文本列表
    Q_INVOKABLE QStringList SearchWindows(const QString &keyword);

    // 把搜索结果加入指定抽屉
    Q_INVOKABLE bool AddSearchResultToDrawer(int resultIndex, int drawerIndex);

    // 激活搜索结果
    Q_INVOKABLE void ActivateSearchResult(int resultIndex);


    // 打开配置文件所在文件夹
    Q_INVOKABLE void OpenConfigFolder();

    // 窗口加入与移出的全局快捷键, 前缀固定为 Ctrl+Alt
    Q_INVOKABLE QString HotkeyAddWindow();
    Q_INVOKABLE QString HotkeyRemoveWindow();
    Q_INVOKABLE bool SaveWindowHotkeys(const QString &addWindow, const QString &removeWindow);

    // 抽屉自身的切换键位, 与全局前缀组合;空表示不设置
    Q_INVOKABLE QString DrawerHotkey(int index) const;
    Q_INVOKABLE bool SaveDrawerHotkey(int index, const QString &key);

    // 把当前桌面的可见窗口批量加入指定抽屉, 返回加入数量
    Q_INVOKABLE int CaptureDesktopToDrawer(int index);

    // 管理窗口尺寸与悬浮窗位置
    Q_INVOKABLE int ManagerWindowWidth();
    Q_INVOKABLE int ManagerWindowHeight();
    Q_INVOKABLE void SaveManagerWindowSize(int width, int height);
    Q_INVOKABLE int FloatingWindowX();
    Q_INVOKABLE int FloatingWindowY();
    Q_INVOKABLE void SaveFloatingPosition(int x, int y);

    // 直接设置内存中的抽屉列表, 不写配置文件, 供自检使用
    void SetDrawersForTest(const QVector<Models::Drawer> &drawers);

signals:
    void drawersChanged();
    void activeIndexChanged();
    void noticeChanged();
    void todosChanged();

    void hotkeySettingsChanged();

private:
    // 判断窗口是否匹配指定抽屉的规则; 同一窗口可匹配多个抽屉
    bool MatchesDrawer(const WindowApi::WindowInfo &info, int index) const;

    // 返回第一个匹配的抽屉索引, 未匹配返回 -1
    int DrawerIndexOf(const WindowApi::WindowInfo &info) const;

    // 保存指定抽屉当前窗口布局
    void CaptureLayout(int index);

    // 恢复指定抽屉的窗口布局
    void RestoreLayout(int index);

    // 按当前活动抽屉调整所有受管窗口的可见性; 同一窗口匹配多个抽屉时以活动抽屉优先
    void ApplyVisibility();

    // 隐藏窗口并记录, 返回是否执行了隐藏
    bool CloakWindow(HWND handle);

    // 恢复窗口并移出记录, 返回是否执行了恢复
    bool UncloakWindow(HWND handle);

    // 把当前隐藏窗口写入状态文件, 供崩溃或脚本恢复
    void PersistCloakedState();

    // 从状态文件恢复仍存在的隐藏窗口, 返回恢复数量
    int RestoreCloakedState();

    // 进程已不存在时清空规则与布局中的 PID, 避免重启后失配
    void PruneStaleProcessIds();
    bool IsProcessAlive(int processId, const QString &processName) const;

    void RefreshNames();
    void Notify(const QString &text);

    QVector<Models::Drawer> m_drawers;
    QStringList m_drawerNames;
    QString m_notice;
    QVector<WindowApi::WindowInfo> m_searchResults;
    QSet<quintptr> m_cloakedWindows;
    QHash<quintptr, WindowApi::WindowInfo> m_cloakedInfo;
    bool m_persistChanges = true;
    bool m_restoring = false;
    int m_activeIndex = 0;
};

}
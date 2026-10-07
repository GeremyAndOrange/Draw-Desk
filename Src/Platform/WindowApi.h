// 文件用途: 窗口平台接口, 提供窗口枚举, 隐藏与显示, 规则匹配与前台跟踪
#pragma once

#include <QRect>
#include <QString>
#include <QVector>

#include <windows.h>

#include "Models/Drawer.h"

namespace DrawDesk::WindowApi {

// 顶层窗口的摘要信息
struct WindowInfo {
    HWND handle = nullptr;
    DWORD processId = 0;
    QString processName;
    QString title;
};

// 枚举有标题的顶层窗口
QVector<WindowInfo> EnumTopLevelWindows();

// 描述单个窗口
WindowInfo DescribeWindow(HWND handle);

// 返回进程可执行文件名, 失败时返回空字符串
QString ProcessNameOf(DWORD processId);

// 窗口布局快照
struct WindowPlacement {
    QRect normalRect;
    int showCommand = 1;
    bool valid = false;
};

// 读取窗口布局
WindowPlacement GetPlacement(HWND handle);

// 应用窗口布局
bool ApplyPlacement(HWND handle, const WindowPlacement &placement);

// 显示器信息
struct MonitorInfo {
    QString name;
    QRect rect;
    bool primary = false;
};

// 返回窗口所在显示器名称, 如 DISPLAY1
QString MonitorNameOf(HWND handle);

// 判断矩形是否位于任一显示器范围
bool IsRectOnAnyMonitor(const QRect &rect);

// 返回主显示器工作区
QRect PrimaryWorkArea();

// 返回窗口的当前外框矩形
QRect WindowRectOf(HWND handle);

// 枚举所有显示器
QVector<MonitorInfo> EnumMonitors();

// 在资源管理器中选中并显示路径
void ShowInExplorer(const QString &path);

// 判断窗口是否属于可管理的应用窗口, includeCloaked 为真时包含已被 DWM 隐藏的窗口
bool IsWindowManageable(HWND handle, bool includeCloaked);

// 判断窗口是否为可抓取的桌面应用窗口
bool IsWindowCapturable(HWND handle);

// 判断窗口是否符合规则
bool MatchesRule(const WindowInfo &info, const Models::WindowRule &rule);

// 查询窗口是否被 DWM 隐藏
bool IsCloaked(HWND handle);

// 设置窗口的 DWM 隐藏状态, 返回调用是否成功
bool SetCloaked(HWND handle, bool cloaked);

// 强制窗口重绘并同步框架, 用于恢复后避免 DWM 残影
void RefreshWindow(HWND handle);

// 等待 DWM 处理完当前合成队列
void FlushComposition();

// 确保窗口可见并激活到前台
void UncloakAndActivate(HWND handle);

// 启动前台窗口监听, 记录最近的非本进程前台窗口
void StartForegroundWatcher();

// 返回最近的外部前台窗口, 可能为空
HWND LastExternalForeground();

// 创建自检用的测试窗口, 失败返回空句柄
HWND CreateTestWindow(const wchar_t *title);

// 销毁测试窗口, 返回是否成功
bool DestroyTestWindow(HWND handle);

}
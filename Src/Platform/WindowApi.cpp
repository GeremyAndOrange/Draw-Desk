// 文件用途: 窗口平台实现, 封装窗口枚举, DWM 隐藏与前台跟踪等 Win32 调用
#include "Platform/WindowApi.h"

#include <QDebug>
#include <QFileInfo>
#include <QRegularExpression>

#include <dwmapi.h>
#include <shellapi.h>

namespace {

HWND g_lastExternalWindow = nullptr;
HWINEVENTHOOK g_foregroundHook = nullptr;

BOOL CALLBACK EnumProc(HWND handle, LPARAM param)
{
    auto *list = reinterpret_cast<QVector<DrawDesk::WindowApi::WindowInfo> *>(param);

    const int titleLength = GetWindowTextLengthW(handle);
    if (titleLength <= 0)
        return TRUE;

    wchar_t titleBuffer[512] = {};
    GetWindowTextW(handle, titleBuffer, 512);

    DWORD processId = 0;
    GetWindowThreadProcessId(handle, &processId);

    DrawDesk::WindowApi::WindowInfo info;
    info.handle = handle;
    info.processId = processId;
    info.processName = DrawDesk::WindowApi::ProcessNameOf(processId);
    info.title = QString::fromWCharArray(titleBuffer);
    list->append(info);
    return TRUE;
}

// 前台窗口变化回调, 记录最近的外部前台窗口
void CALLBACK ForegroundEventProc(HWINEVENTHOOK, DWORD event, HWND handle, LONG, LONG, DWORD,
                                  DWORD)
{
    if (event != EVENT_SYSTEM_FOREGROUND || !handle)
        return;

    DWORD processId = 0;
    GetWindowThreadProcessId(handle, &processId);
    if (processId == GetCurrentProcessId())
        return;

    g_lastExternalWindow = handle;
}

// 注册一次测试窗口类
bool EnsureTestWindowClass()
{
    static bool registered = false;
    if (registered)
        return true;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"DrawDeskSelfTestWindow";

    if (!RegisterClassExW(&wc))
        return false;

    registered = true;
    return true;
}

}

namespace DrawDesk::WindowApi {

QVector<WindowInfo> EnumTopLevelWindows()
{
    QVector<WindowInfo> list;
    EnumWindows(EnumProc, reinterpret_cast<LPARAM>(&list));
    return list;
}

WindowInfo DescribeWindow(HWND handle)
{
    WindowInfo info;
    if (!handle)
        return info;

    info.handle = handle;
    GetWindowThreadProcessId(handle, &info.processId);
    info.processName = ProcessNameOf(info.processId);

    const int titleLength = GetWindowTextLengthW(handle);
    if (titleLength > 0) {
        wchar_t titleBuffer[512] = {};
        GetWindowTextW(handle, titleBuffer, 512);
        info.title = QString::fromWCharArray(titleBuffer);
    }
    return info;
}

QString ProcessNameOf(DWORD processId)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process)
        return QString();

    wchar_t pathBuffer[1024] = {};
    DWORD size = 1024;
    const bool ok = QueryFullProcessImageNameW(process, 0, pathBuffer, &size);
    CloseHandle(process);

    if (!ok)
        return QString();

    return QFileInfo(QString::fromWCharArray(pathBuffer)).fileName();
}

WindowPlacement GetPlacement(HWND handle)
{
    WindowPlacement result;
    if (!handle)
        return result;

    WINDOWPLACEMENT native = {};
    native.length = sizeof(native);
    if (!GetWindowPlacement(handle, &native))
        return result;

    result.valid = true;
    result.normalRect = QRect(native.rcNormalPosition.left, native.rcNormalPosition.top,
                              native.rcNormalPosition.right - native.rcNormalPosition.left,
                              native.rcNormalPosition.bottom - native.rcNormalPosition.top);
    result.showCommand = native.showCmd;
    return result;
}

bool ApplyPlacement(HWND handle, const WindowPlacement &placement)
{
    if (!handle || !placement.valid)
        return false;

    WINDOWPLACEMENT native = {};
    native.length = sizeof(native);
    native.rcNormalPosition.left = placement.normalRect.left();
    native.rcNormalPosition.top = placement.normalRect.top();
    native.rcNormalPosition.right = placement.normalRect.right() + 1;
    native.rcNormalPosition.bottom = placement.normalRect.bottom() + 1;
    native.showCmd = placement.showCommand;

    return SetWindowPlacement(handle, &native) != FALSE;
}

BOOL CALLBACK MonitorEnumProc(HMONITOR monitor, HDC, LPRECT, LPARAM param)
{
    auto *list = reinterpret_cast<QVector<DrawDesk::WindowApi::MonitorInfo> *>(param);

    MONITORINFOEXW info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info))
        return TRUE;

    DrawDesk::WindowApi::MonitorInfo item;
    item.name = QString::fromWCharArray(info.szDevice);
    item.name.remove(QStringLiteral("\\\\.\\"));
    item.rect = QRect(info.rcMonitor.left, info.rcMonitor.top,
                      info.rcMonitor.right - info.rcMonitor.left,
                      info.rcMonitor.bottom - info.rcMonitor.top);
    item.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    list->append(item);
    return TRUE;
}

QRect WindowRectOf(HWND handle)
{
    if (!handle)
        return QRect();

    RECT rect = {};
    if (!GetWindowRect(handle, &rect))
        return QRect();

    return QRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}

QVector<MonitorInfo> EnumMonitors()
{
    QVector<MonitorInfo> list;
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&list));
    return list;
}

void ShowInExplorer(const QString &path)
{
    if (path.isEmpty())
        return;

    QString nativePath = path;
    nativePath.replace(QLatin1Char('/'), QLatin1Char('\\'));

    std::wstring arguments = L"/select,\"";
    arguments += nativePath.toStdWString();
    arguments += L"\"";

    ShellExecuteW(nullptr, L"open", L"explorer.exe", arguments.c_str(), nullptr, SW_SHOWNORMAL);
}

bool IsWindowManageable(HWND handle, bool includeCloaked)
{
    if (!handle || !IsWindowVisible(handle))
        return false;

    if (GetWindowTextLengthW(handle) <= 0)
        return false;

    const LONG exStyle = GetWindowLongW(handle, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW)
        return false;

    wchar_t className[128] = {};
    GetClassNameW(handle, className, 128);
    const QString name = QString::fromWCharArray(className);
    if (name == QStringLiteral("Progman") || name == QStringLiteral("WorkerW")
        || name == QStringLiteral("Shell_TrayWnd") || name == QStringLiteral("Shell_SecondaryTrayWnd")
        || name == QStringLiteral("TaskListThumbnailWnd")
        || name == QStringLiteral("ForegroundStaging")
        || name == QStringLiteral("XamlExplorerHostIslandWindow")
        || name == QStringLiteral("Windows.UI.Core.CoreWindow"))
        return false;

    DWORD processId = 0;
    GetWindowThreadProcessId(handle, &processId);
    if (processId == GetCurrentProcessId())
        return false;

    // UWP 窗口由 ApplicationFrameHost 承载, DWM Cloak 行为不稳定, 不参与管理
    const QString processName = ProcessNameOf(processId);
    if (QString::compare(processName, QStringLiteral("ApplicationFrameHost.exe"),
                         Qt::CaseInsensitive)
        == 0)
        return false;

    if (!includeCloaked && IsCloaked(handle))
        return false;

    return true;
}

bool IsWindowCapturable(HWND handle)
{
    return IsWindowManageable(handle, false);
}

QString MonitorNameOf(HWND handle)
{
    if (!handle)
        return QString();

    const HMONITOR monitor = MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXW monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!monitor || !GetMonitorInfoW(monitor, &monitorInfo))
        return QString();

    QString name = QString::fromWCharArray(monitorInfo.szDevice);
    name.remove(QStringLiteral("\\\\.\\"));
    return name;
}

bool IsRectOnAnyMonitor(const QRect &rect)
{
    RECT native = {};
    native.left = rect.left();
    native.top = rect.top();
    native.right = rect.right() + 1;
    native.bottom = rect.bottom() + 1;

    return MonitorFromRect(&native, MONITOR_DEFAULTTONULL) != nullptr;
}

QRect PrimaryWorkArea()
{
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    const HMONITOR monitor = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    if (!monitor || !GetMonitorInfoW(monitor, &info)) {
        return QRect(0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    }

    const RECT &work = info.rcWork;
    return QRect(work.left, work.top, work.right - work.left, work.bottom - work.top);
}

bool MatchesRule(const WindowInfo &info, const Models::WindowRule &rule)
{
    if (rule.process.isEmpty())
        return false;

    if (QString::compare(info.processName, rule.process, Qt::CaseInsensitive) != 0)
        return false;

    if (rule.processId > 0 && info.processId != static_cast<DWORD>(rule.processId))
        return false;

    if (rule.titlePattern.isEmpty())
        return true;

    // 标题先按字面精确比较;只有显式开启正则匹配时才按正则处理
    if (info.title == rule.titlePattern)
        return true;
    if (!rule.useRegex)
        return false;

    const QRegularExpression expression(rule.titlePattern);
    if (!expression.isValid())
        return false;

    return expression.match(info.title).hasMatch();
}

bool IsCloaked(HWND handle)
{
    if (!handle)
        return false;

    DWORD cloaked = 0;
    const HRESULT result = DwmGetWindowAttribute(handle, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    return SUCCEEDED(result) && cloaked != 0;
}

bool SetCloaked(HWND handle, bool cloaked)
{
    if (!handle)
        return false;

    const BOOL value = cloaked ? TRUE : FALSE;
    const HRESULT result = DwmSetWindowAttribute(handle, DWMWA_CLOAK, &value, sizeof(value));
    if (SUCCEEDED(result)) {
        if (!cloaked) {
            // 恢复时, 若窗口曾被回退方式隐藏, 需要重新显示
            if (!IsWindowVisible(handle))
                ShowWindow(handle, SW_SHOWNA);
            RefreshWindow(handle);
        }
        return true;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(handle, &processId);
    const QString processName = ProcessNameOf(processId);
    qWarning().noquote()
        << QStringLiteral("DWM %1调用失败, 进程: %2, 句柄: %3, HRESULT: 0x%4")
               .arg(cloaked ? QStringLiteral("隐藏") : QStringLiteral("恢复"), processName,
                    QString::number(reinterpret_cast<quintptr>(handle)),
                    QString::number(static_cast<quint32>(result), 16));

    // 回退方案: 使用标准显示与隐藏, 恢复后强制刷新, 减少 DWM 合成残影
    ShowWindow(handle, cloaked ? SW_HIDE : SW_SHOWNA);
    if (!cloaked)
        RefreshWindow(handle);
    return true;
}

void RefreshWindow(HWND handle)
{
    if (!handle || !IsWindow(handle))
        return;

    // 异步提交框架变化, 避免目标窗口不响应时阻塞当前线程
    SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
                     | SWP_FRAMECHANGED | SWP_ASYNCWINDOWPOS | SWP_SHOWWINDOW);
    RedrawWindow(handle, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN | RDW_ERASE);
}

void FlushComposition()
{
    DwmFlush();
}

void UncloakAndActivate(HWND handle)
{
    if (!handle)
        return;

    const BOOL value = FALSE;
    DwmSetWindowAttribute(handle, DWMWA_CLOAK, &value, sizeof(value));

    if (!IsWindowVisible(handle))
        ShowWindow(handle, SW_SHOWNA);

    if (IsIconic(handle))
        ShowWindow(handle, SW_RESTORE);

    RefreshWindow(handle);
    SetForegroundWindow(handle);
}


void StartForegroundWatcher()
{
    if (g_foregroundHook)
        return;

    g_foregroundHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
                                       ForegroundEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);

    const HWND current = GetForegroundWindow();
    if (current) {
        DWORD processId = 0;
        GetWindowThreadProcessId(current, &processId);
        if (processId != GetCurrentProcessId())
            g_lastExternalWindow = current;
    }
}

HWND LastExternalForeground()
{
    if (g_lastExternalWindow && IsWindow(g_lastExternalWindow))
        return g_lastExternalWindow;

    return nullptr;
}

HWND CreateTestWindow(const wchar_t *title)
{
    if (!EnsureTestWindowClass())
        return nullptr;

    HWND handle = CreateWindowExW(0, L"DrawDeskSelfTestWindow", title, WS_OVERLAPPEDWINDOW, 200,
                                  200, 360, 160, nullptr, nullptr, GetModuleHandleW(nullptr),
                                  nullptr);
    if (!handle)
        return nullptr;

    ShowWindow(handle, SW_SHOW);
    UpdateWindow(handle);
    return handle;
}

bool DestroyTestWindow(HWND handle)
{
    if (!handle)
        return false;

    return DestroyWindow(handle) != FALSE;
}

}
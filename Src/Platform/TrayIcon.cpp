// 文件用途: 托盘图标实现, 使用 Shell_NotifyIcon 与隐藏消息窗口
#include "Platform/TrayIcon.h"

#include <shellapi.h>

namespace {

constexpr UINT CALLBACK_MESSAGE = WM_APP + 100;
constexpr int MENU_ID_FLOATING = 2;

constexpr int MENU_ID_QUIT = 4;

HWND g_window = nullptr;
NOTIFYICONDATAW g_iconData = {};
UINT g_taskbarCreatedMessage = 0;
DrawDesk::TrayIcon::Callbacks g_callbacks;

// 弹出托盘右键菜单
void ShowContextMenu()
{
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return;

    AppendMenuW(menu, MF_STRING, MENU_ID_FLOATING, L"显示/隐藏悬浮窗");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, MENU_ID_QUIT, L"退出 DrawDesk");

    POINT point = {};
    GetCursorPos(&point);

    // 先设置前台窗口, 避免菜单点击外部后不消失
    SetForegroundWindow(g_window);
    const int command = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                                       point.x, point.y, 0, g_window, nullptr);
    PostMessageW(g_window, WM_NULL, 0, 0);
    DestroyMenu(menu);

    if (command == MENU_ID_FLOATING && g_callbacks.onToggleFloating)
        g_callbacks.onToggleFloating();
    else if (command == MENU_ID_QUIT && g_callbacks.onQuit)
        g_callbacks.onQuit();
}

// 托盘消息窗口的处理过程
LRESULT CALLBACK WndProc(HWND handle, UINT message, WPARAM wparam, LPARAM lparam)
{
    // 资源管理器重启后重新添加托盘图标
    if (g_taskbarCreatedMessage != 0 && message == g_taskbarCreatedMessage) {
        Shell_NotifyIconW(NIM_ADD, &g_iconData);
        return 0;
    }

    if (message == CALLBACK_MESSAGE) {
        switch (LOWORD(lparam)) {
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowContextMenu();
            break;
        case WM_LBUTTONUP:
            if (g_callbacks.onShowMainWindow)
                g_callbacks.onShowMainWindow();
            break;
        default:
            break;
        }
        return 0;
    }

    return DefWindowProcW(handle, message, wparam, lparam);
}

// 注册托盘消息窗口类
bool EnsureWindowClass(HINSTANCE instance)
{
    static bool registered = false;
    if (registered)
        return true;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"DrawDeskTrayWindow";

    if (!RegisterClassExW(&wc))
        return false;

    registered = true;
    return true;
}

}

namespace DrawDesk::TrayIcon {

bool Create(HINSTANCE instance, HICON icon, Callbacks callbacks)
{
    if (g_window)
        return true;

    if (!EnsureWindowClass(instance))
        return false;

    g_window = CreateWindowExW(0, L"DrawDeskTrayWindow", L"", 0, 0, 0, 0, 0, nullptr, nullptr,
                               instance, nullptr);
    if (!g_window)
        return false;

    g_callbacks = std::move(callbacks);
    g_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");

    g_iconData = {};
    g_iconData.cbSize = sizeof(g_iconData);
    g_iconData.hWnd = g_window;
    g_iconData.uID = 1;
    g_iconData.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    g_iconData.uCallbackMessage = CALLBACK_MESSAGE;
    g_iconData.hIcon = icon ? icon : LoadIconW(nullptr, IDI_APPLICATION);
    lstrcpynW(g_iconData.szTip, L"DrawDesk - 窗口抽屉", 128);

    return Shell_NotifyIconW(NIM_ADD, &g_iconData) != FALSE;
}


void Destroy()
{
    if (!g_window)
        return;

    Shell_NotifyIconW(NIM_DELETE, &g_iconData);
    DestroyWindow(g_window);
    g_window = nullptr;
    g_callbacks = {};
}

}
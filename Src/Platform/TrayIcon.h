// 文件用途: 托盘图标接口, 基于 Win32 Shell_NotifyIcon 实现
#pragma once

#include <functional>

#include <windows.h>

namespace DrawDesk::TrayIcon {

// 托盘菜单回调集合
struct Callbacks {
    std::function<void()> onShowMainWindow;
    std::function<void()> onToggleFloating;
    std::function<void()> onQuit;
};

// 创建托盘图标与隐藏消息窗口
bool Create(HINSTANCE instance, HICON icon, Callbacks callbacks);

// 移除托盘图标并销毁消息窗口
void Destroy();

}
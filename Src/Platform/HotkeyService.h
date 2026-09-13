// 文件用途: 全局热键接口, 注册热键并在按下时回调
#pragma once

#include <QString>

#include <functional>

#include <windows.h>

namespace DrawDesk::HotkeyService {

using Callback = std::function<void()>;

// 创建隐藏的消息窗口, 成功返回 true
bool Start();

// 注册全局热键, 修饰键与虚拟键使用 Win32 常量, 成功返回 true
bool Register(int id, UINT modifiers, UINT virtualKey, Callback callback);

// 查询热键是否已注册
bool IsRegistered(int id);

// 注销全部热键并销毁隐藏窗口
void Stop();

// 解析修饰键部分, 如 Ctrl+Alt
bool ParseModifiers(const QString &text, UINT *modifiers);

// 解析完整热键, 如 Ctrl+Alt+0
bool ParseHotkey(const QString &text, UINT *modifiers, UINT *virtualKey);

}

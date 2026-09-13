// 文件用途: 全局热键实现, 通过隐藏窗口接收 WM_HOTKEY 消息
#include "Platform/HotkeyService.h"

#include <QHash>
#include <QStringList>
#include <QString>

namespace {

constexpr const wchar_t *WINDOW_CLASS = L"DrawDeskHotkeyWindow";

HWND g_window = nullptr;
QHash<int, DrawDesk::HotkeyService::Callback> g_callbacks;

// 隐藏窗口的消息处理, 分发热键回调
LRESULT CALLBACK WndProc(HWND handle, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_HOTKEY) {
        const int id = static_cast<int>(wparam);
        const auto it = g_callbacks.constFind(id);
        if (it != g_callbacks.constEnd() && *it)
            (*it)();
        return 0;
    }
    return DefWindowProcW(handle, message, wparam, lparam);
}

// 注册隐藏窗口类
bool EnsureWindowClass()
{
    static bool registered = false;
    if (registered)
        return true;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = WINDOW_CLASS;

    if (!RegisterClassExW(&wc))
        return false;

    registered = true;
    return true;
}

}

namespace DrawDesk::HotkeyService {

bool ParseModifiers(const QString &text, UINT *modifiers)
{
    UINT mods = 0;
    const QStringList parts = text.split(QLatin1Char('+'), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return false;

    for (const QString &partRaw : parts) {
        const QString part = partRaw.trimmed().toLower();
        if (part == QStringLiteral("ctrl") || part == QStringLiteral("control"))
            mods |= MOD_CONTROL;
        else if (part == QStringLiteral("alt"))
            mods |= MOD_ALT;
        else if (part == QStringLiteral("shift"))
            mods |= MOD_SHIFT;
        else if (part == QStringLiteral("win"))
            mods |= MOD_WIN;
        else
            return false;
    }

    if (mods == 0)
        return false;

    *modifiers = mods;
    return true;
}

bool ParseHotkey(const QString &text, UINT *modifiers, UINT *virtualKey)
{
    const QStringList parts = text.split(QLatin1Char('+'), Qt::SkipEmptyParts);
    if (parts.size() < 2)
        return false;

    UINT mods = 0;
    for (int i = 0; i < parts.size() - 1; ++i) {
        const QString part = parts.at(i).trimmed().toLower();
        if (part == QStringLiteral("ctrl") || part == QStringLiteral("control"))
            mods |= MOD_CONTROL;
        else if (part == QStringLiteral("alt"))
            mods |= MOD_ALT;
        else if (part == QStringLiteral("shift"))
            mods |= MOD_SHIFT;
        else if (part == QStringLiteral("win"))
            mods |= MOD_WIN;
        else
            return false;
    }

    if (mods == 0)
        return false;

    const QString key = parts.last().trimmed().toUpper();
    UINT vk = 0;
    if (key.size() == 1 && key.at(0).isLetterOrNumber()) {
        vk = key.at(0).unicode();
    } else if (key.size() == 1) {
        switch (key.at(0).unicode()) {
        case '-':
            vk = VK_OEM_MINUS;
            break;
        case '=':
            vk = VK_OEM_PLUS;
            break;
        case '[':
            vk = VK_OEM_4;
            break;
        case ']':
            vk = VK_OEM_6;
            break;
        case ';':
            vk = VK_OEM_1;
            break;
        case '\'':
            vk = VK_OEM_7;
            break;
        case ',':
            vk = VK_OEM_COMMA;
            break;
        case '.':
            vk = VK_OEM_PERIOD;
            break;
        case '/':
            vk = VK_OEM_2;
            break;
        case '\\':
            vk = VK_OEM_5;
            break;
        case '`':
            vk = VK_OEM_3;
            break;
        default:
            return false;
        }
    } else if (key == QStringLiteral("SPACE")) {
        vk = VK_SPACE;
    } else if (key.startsWith(QLatin1Char('F')) && key.size() <= 3) {
        bool ok = false;
        const int number = key.mid(1).toInt(&ok);
        if (!ok || number < 1 || number > 12)
            return false;
        vk = VK_F1 + number - 1;
    } else {
        return false;
    }

    if (vk == 0)
        return false;

    *modifiers = mods;
    *virtualKey = vk;
    return true;
}


bool Start()
{
    if (g_window)
        return true;

    if (!EnsureWindowClass())
        return false;

    g_window = CreateWindowExW(0, WINDOW_CLASS, L"", 0, 0, 0, 0, 0, nullptr, nullptr,
                               GetModuleHandleW(nullptr), nullptr);
    return g_window != nullptr;
}

bool Register(int id, UINT modifiers, UINT virtualKey, Callback callback)
{
    if (!g_window)
        return false;

    if (!RegisterHotKey(g_window, id, modifiers | MOD_NOREPEAT, virtualKey))
        return false;

    g_callbacks.insert(id, std::move(callback));
    return true;
}

bool IsRegistered(int id)
{
    return g_callbacks.contains(id);
}

void Stop()
{
    if (!g_window)
        return;

    for (auto it = g_callbacks.constBegin(); it != g_callbacks.constEnd(); ++it)
        UnregisterHotKey(g_window, it.key());

    g_callbacks.clear();
    DestroyWindow(g_window);
    g_window = nullptr;
}

}

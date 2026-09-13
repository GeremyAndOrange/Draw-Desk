// 文件用途: 单实例实现, 使用命名互斥体进行进程间互斥
#include "Platform/SingleInstance.h"

#include <windows.h>

namespace {

constexpr const wchar_t *MUTEX_NAME = L"Local\\DrawDesk.SingleInstance";
HANDLE g_mutex = nullptr;

}

namespace DrawDesk::SingleInstance {

bool Acquire()
{
    SetLastError(ERROR_SUCCESS);
    HANDLE mutex = CreateMutexW(nullptr, FALSE, MUTEX_NAME);
    if (!mutex)
        return false;

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return false;
    }

    g_mutex = mutex;
    return true;
}

bool VerifyDetectable()
{
    SetLastError(ERROR_SUCCESS);
    HANDLE probe = CreateMutexW(nullptr, FALSE, MUTEX_NAME);
    if (!probe)
        return false;

    const bool detected = (GetLastError() == ERROR_ALREADY_EXISTS);
    CloseHandle(probe);
    return detected;
}

}

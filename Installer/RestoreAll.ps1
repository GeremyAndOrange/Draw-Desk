# 文件用途: 独立恢复脚本, 读取 DrawDesk 隐藏状态并把被隐藏的窗口重新显示
param(
    [string]$DataDir = (Join-Path $env:APPDATA 'DrawDesk\DrawDesk'),
    [switch]$All
)

$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class DrawDeskRestore
{
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowTextLengthW(IntPtr hWnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowTextW(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    [DllImport("user32.dll")]
    public static extern bool IsWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("dwmapi.dll")]
    public static extern int DwmGetWindowAttribute(IntPtr hWnd, int attribute, out int value, int size);

    [DllImport("dwmapi.dll")]
    public static extern int DwmSetWindowAttribute(IntPtr hWnd, int attribute, ref int value, int size);
}
"@

function Get-WindowTitle([IntPtr]$handle) {
    $length = [DrawDeskRestore]::GetWindowTextLengthW($handle)
    if ($length -le 0) {
        return ''
    }
    $buffer = New-Object System.Text.StringBuilder ($length + 1)
    [void][DrawDeskRestore]::GetWindowTextW($handle, $buffer, $buffer.Capacity)
    return $buffer.ToString()
}

function Get-ProcessNameSafe([uint32]$processId) {
    try {
        return (Get-Process -Id $processId -ErrorAction Stop).ProcessName
    } catch {
        return ''
    }
}

function Test-ProcessName([string]$expected, [string]$actual) {
    if ([string]::IsNullOrEmpty($expected)) {
        return $true
    }
    $expectedBase = [System.IO.Path]::GetFileNameWithoutExtension($expected)
    $actualBase = [System.IO.Path]::GetFileNameWithoutExtension($actual)
    return [string]::Equals($expectedBase, $actualBase, [System.StringComparison]::OrdinalIgnoreCase)
}

function Get-TopLevelWindows {
    $list = New-Object System.Collections.ArrayList

    $callback = [DrawDeskRestore+EnumWindowsProc] {
        param([IntPtr]$handle, [IntPtr]$param)

        $length = [DrawDeskRestore]::GetWindowTextLengthW($handle)
        if ($length -le 0) {
            return $true
        }

        [uint32]$processId = 0
        [void][DrawDeskRestore]::GetWindowThreadProcessId($handle, [ref]$processId)

        [int]$cloaked = 0
        [void][DrawDeskRestore]::DwmGetWindowAttribute($handle, 14, [ref]$cloaked, 4)

        $item = [PSCustomObject]@{
            Handle   = $handle
            ProcessId = [int]$processId
            Process  = Get-ProcessNameSafe $processId
            Title    = Get-WindowTitle $handle
            Cloaked  = ($cloaked -ne 0)
            Visible  = [DrawDeskRestore]::IsWindowVisible($handle)
        }
        [void]$list.Add($item)
        return $true
    }

    [void][DrawDeskRestore]::EnumWindows($callback, [IntPtr]::Zero)
    return $list
}

function Restore-Window([IntPtr]$handle) {
    [int]$zero = 0
    [void][DrawDeskRestore]::DwmSetWindowAttribute($handle, 14, [ref]$zero, 4)
    if (-not [DrawDeskRestore]::IsWindowVisible($handle)) {
        [void][DrawDeskRestore]::ShowWindow($handle, 8)
    }
}

# -All 为强制模式: 不读状态文件, 直接尝试恢复所有处于 Cloak 或不可见状态的带标题窗口
if ($All) {
    $windows = @(Get-TopLevelWindows)
    $count = 0
    foreach ($window in $windows) {
        if ($window.Cloaked -or -not $window.Visible) {
            Restore-Window $window.Handle
            $count += 1
        }
    }
    Write-Host "强制恢复完成: 显示 $count 个窗口."
    exit 0
}
$statePath = Join-Path $DataDir 'Cloaked.json'
if (-not (Test-Path -LiteralPath $statePath)) {
    Write-Host "没有找到隐藏状态文件: $statePath"
    Write-Host '没有需要恢复的窗口.'
    exit 0
}

try {
    $state = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
} catch {
    throw "隐藏状态文件格式无效: $statePath"
}

$entries = @($state.windows)
if ($entries.Count -eq 0) {
    Remove-Item -LiteralPath $statePath -Force -ErrorAction SilentlyContinue
    Write-Host '隐藏状态文件为空, 没有需要恢复的窗口.'
    exit 0
}

$windows = @(Get-TopLevelWindows)
$remaining = New-Object System.Collections.ArrayList
$restored = 0

foreach ($entry in $entries) {
    $done = $false
    $entryProcessId = 0
    if ($null -ne $entry.processId) {
        $entryProcessId = [int]$entry.processId
    }

    $rawHandle = [long]0
    try {
        $rawHandle = [System.Convert]::ToInt64([string]$entry.handle, 16)
    } catch {
        $rawHandle = 0
    }

    # 先按句柄恢复, 窗口标题变化也能命中
    if ($rawHandle -ne 0) {
        $handle = [IntPtr]$rawHandle
        if ([DrawDeskRestore]::IsWindow($handle)) {
            [uint32]$windowProcessId = 0
            [void][DrawDeskRestore]::GetWindowThreadProcessId($handle, [ref]$windowProcessId)
            $processMatch = Test-ProcessName ([string]$entry.process) (Get-ProcessNameSafe $windowProcessId)
            $idMatch = ($entryProcessId -eq 0 -or $entryProcessId -eq [int]$windowProcessId)
            if ($processMatch -and $idMatch) {
                Restore-Window $handle
                $restored += 1
                $done = $true
            }
        }
    }

    # 句柄失效时按进程号与标题精确匹配
    if (-not $done) {
        foreach ($window in $windows) {
            if ($entryProcessId -ne 0 -and $entryProcessId -ne $window.ProcessId) {
                continue
            }
            if (-not (Test-ProcessName ([string]$entry.process) $window.Process)) {
                continue
            }
            if (-not [string]::IsNullOrEmpty($entry.title) -and $entry.title -ne $window.Title) {
                continue
            }
            Restore-Window $window.Handle
            $restored += 1
            $done = $true
            break
        }
    }

    # 最后按进程匹配当前仍然处于隐藏或 Cloak 状态的窗口
    if (-not $done) {
        foreach ($window in $windows) {
            if (-not (Test-ProcessName ([string]$entry.process) $window.Process)) {
                continue
            }
            if (-not ($window.Cloaked -or -not $window.Visible)) {
                continue
            }
            Restore-Window $window.Handle
            $restored += 1
            $done = $true
            break
        }
    }

    if (-not $done) {
        [void]$remaining.Add($entry)
    }
}

if ($remaining.Count -eq 0) {
    Remove-Item -LiteralPath $statePath -Force -ErrorAction SilentlyContinue
} else {
    $state.windows = @($remaining)
    $state | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $statePath -Encoding UTF8
}

Write-Host "恢复完成: 显示 $restored 个窗口, 未匹配 $($remaining.Count) 个."
if ($remaining.Count -gt 0) {
    Write-Host '未匹配的窗口可能已经关闭, 状态文件已保留.'
}
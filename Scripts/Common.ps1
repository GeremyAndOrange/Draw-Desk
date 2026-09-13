# 文件用途: 解析 Qt 与 MinGW 安装路径, 供构建, 部署与运行脚本共用
function Resolve-QtRoot {
    param([string]$QtRoot)

    $candidates = New-Object System.Collections.ArrayList
    if ($QtRoot) { [void]$candidates.Add($QtRoot) }
    if ($env:QT_ROOT) { [void]$candidates.Add($env:QT_ROOT) }
    if ($env:QTDIR) { [void]$candidates.Add($env:QTDIR) }
    [void]$candidates.AddRange(@(
        'C:\Qt\6.11.2\mingw_64',
        'D:\Qt\6.11.2\mingw_64',
        (Join-Path $env:USERPROFILE 'Qt\6.11.2\mingw_64')
    ))

    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) { continue }
        $qmake = Join-Path $candidate 'bin\qmake.exe'
        if (Test-Path -LiteralPath $qmake) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw '未找到 Qt 6.11.2 MinGW 安装目录. 请设置环境变量 QT_ROOT, 例如: $env:QT_ROOT = ''C:\Qt\6.11.2\mingw_64'''
}

function Resolve-MingwBin {
    param([string]$MingwBin, [string]$QtRoot)

    $candidates = New-Object System.Collections.ArrayList
    if ($MingwBin) { [void]$candidates.Add($MingwBin) }
    if ($env:MINGW_BIN) { [void]$candidates.Add($env:MINGW_BIN) }

    if ($QtRoot) {
        $toolsDir = Join-Path (Split-Path (Split-Path $QtRoot -Parent) -Parent) 'Tools'
        if (Test-Path -LiteralPath $toolsDir) {
            Get-ChildItem -LiteralPath $toolsDir -Directory -Filter 'mingw*' -ErrorAction SilentlyContinue |
                ForEach-Object { [void]$candidates.Add((Join-Path $_.FullName 'bin')) }
        }
    }

    [void]$candidates.AddRange(@(
        'C:\Qt\Tools\mingw1310_64\bin',
        'D:\Qt\Tools\mingw1310_64\bin'
    ))

    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) { continue }
        if (Test-Path -LiteralPath (Join-Path $candidate 'g++.exe')) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw '未找到 MinGW 工具链. 请设置环境变量 MINGW_BIN, 例如: $env:MINGW_BIN = ''C:\Qt\Tools\mingw1310_64\bin'''
}

function Resolve-CMake {
    param([string]$QtRoot)

    $candidates = New-Object System.Collections.ArrayList
    if ($env:QT_CMAKE) { [void]$candidates.Add($env:QT_CMAKE) }

    if ($QtRoot) {
        $toolsDir = Join-Path (Split-Path (Split-Path $QtRoot -Parent) -Parent) 'Tools'
        if (Test-Path -LiteralPath $toolsDir) {
            Get-ChildItem -LiteralPath $toolsDir -Directory -Filter 'CMake*' -ErrorAction SilentlyContinue |
                ForEach-Object { [void]$candidates.Add((Join-Path $_.FullName 'bin\cmake.exe')) }
        }
    }

    [void]$candidates.AddRange(@(
        'C:\Qt\Tools\CMake_64\bin\cmake.exe',
        'D:\Qt\Tools\CMake_64\bin\cmake.exe'
    ))

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    throw '未找到 CMake. 请设置环境变量 QT_CMAKE, 或把 CMake 加入 PATH'
}

function Initialize-DrawDeskToolchain {
    param([string]$QtRoot, [string]$MingwBin)

    $resolvedQt = Resolve-QtRoot $QtRoot
    $resolvedMingw = Resolve-MingwBin -MingwBin $MingwBin -QtRoot $resolvedQt
    $resolvedCMake = Resolve-CMake $resolvedQt

    # CMake 预设里使用正斜杠, 避免反斜杠在预设表达式中被转义
    $env:QT_ROOT = $resolvedQt.Replace('\', '/')
    $env:MINGW_BIN = $resolvedMingw.Replace('\', '/')
    $env:Path = "$resolvedMingw;$resolvedQt\bin;$env:Path"

    return [PSCustomObject]@{
        QtRoot = $resolvedQt
        MingwBin = $resolvedMingw
        CMake = $resolvedCMake
    }
}
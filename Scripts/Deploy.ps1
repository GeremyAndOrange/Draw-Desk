# 文件用途: 部署脚本, 复制 Qt 运行库与 QML 模块到构建输出目录, 使 exe 可独立运行
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Debug',
    [string]$QtRoot = '',
    [string]$MingwBin = '',
    [switch]$FullQml
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\Common.ps1"

$root = Split-Path -Parent $PSScriptRoot
$toolchain = Initialize-DrawDeskToolchain -QtRoot $QtRoot -MingwBin $MingwBin
$name = $Config.ToLower()
$target = Join-Path $root "Build\$name"
$exe = Join-Path $target 'DrawDesk.exe'
if (-not (Test-Path $exe)) {
    throw "未找到可执行文件, 请先运行 Build.ps1: $exe"
}

$qtRoot = $toolchain.QtRoot
$qtBin = Join-Path $qtRoot 'bin'
$objdump = Join-Path $toolchain.MingwBin 'objdump.exe'

# 复制 QML 模块与平台插件, 先清理旧目录避免嵌套复制
$qmlTarget = Join-Path $target 'qml'
$platformsTarget = Join-Path $target 'platforms'
$imageFormatsTarget = Join-Path $target 'imageformats'
$stylesTarget = Join-Path $target 'styles'
Remove-Item $qmlTarget, $platformsTarget, $imageFormatsTarget, $stylesTarget -Recurse -Force -ErrorAction SilentlyContinue
Get-ChildItem $target -Filter '*.dll' -File -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue

if ($FullQml) {
    Copy-Item (Join-Path $qtRoot 'qml') $qmlTarget -Recurse -Force
} else {
    # 默认只复制程序实际用到的 QML 模块, 需要完整复制时加 -FullQml
    New-Item -ItemType Directory $qmlTarget -Force | Out-Null
    Copy-Item (Join-Path $qtRoot 'qml\QML') (Join-Path $qmlTarget 'QML') -Recurse -Force

    $qtQmlTarget = Join-Path $qmlTarget 'QtQml'
    New-Item -ItemType Directory $qtQmlTarget -Force | Out-Null
    Get-ChildItem (Join-Path $qtRoot 'qml\QtQml') -File | ForEach-Object { Copy-Item $_.FullName $qtQmlTarget -Force }
    foreach ($sub in @('Models', 'WorkerScript')) {
        $source = Join-Path $qtRoot "qml\QtQml\$sub"
        if (Test-Path $source) {
            Copy-Item $source (Join-Path $qtQmlTarget $sub) -Recurse -Force
        }
    }

    $qtQuickTarget = Join-Path $qmlTarget 'QtQuick'
    New-Item -ItemType Directory $qtQuickTarget -Force | Out-Null
    Get-ChildItem (Join-Path $qtRoot 'qml\QtQuick') -File | ForEach-Object { Copy-Item $_.FullName $qtQuickTarget -Force }
    foreach ($sub in @('Effects', 'Window')) {
        $source = Join-Path $qtRoot "qml\QtQuick\$sub"
        if (Test-Path $source) {
            Copy-Item $source (Join-Path $qtQuickTarget $sub) -Recurse -Force
        }
    }
}
New-Item -ItemType Directory (Join-Path $target 'platforms') -Force | Out-Null
Copy-Item (Join-Path $qtRoot 'plugins\platforms\qwindows.dll') (Join-Path $target 'platforms') -Force
Copy-Item (Join-Path $qtRoot 'plugins\imageformats') $imageFormatsTarget -Recurse -Force

# 解析依赖闭包, 复制 Qt 与 MinGW 运行时 DLL
$seen = @{}
$queue = [System.Collections.Queue]::new()
$queue.Enqueue($exe)
$pluginRoots = @(
    (Join-Path $target 'qml'),
    (Join-Path $target 'platforms'),
    (Join-Path $target 'imageformats')
)
foreach ($plugin in (Get-ChildItem $pluginRoots -Recurse -Filter *.dll -ErrorAction SilentlyContinue)) {
    $queue.Enqueue($plugin.FullName)
}

while ($queue.Count -gt 0) {
    $file = $queue.Dequeue()
    $names = & $objdump -p $file 2>$null | Select-String 'DLL Name' | ForEach-Object {
        ($_.Line -split ':', 2)[1].Trim()
    }
    foreach ($dll in $names) {
        $key = $dll.ToLower()
        if ($seen.ContainsKey($key)) { continue }
        $seen[$key] = $true

        $source = $null
        foreach ($dir in @($qtBin, $toolchain.MingwBin)) {
            $candidate = Join-Path $dir $dll
            if (Test-Path $candidate) { $source = $candidate; break }
        }
        if (-not $source) { continue }

        $destination = Join-Path $target $dll
        if (-not (Test-Path $destination)) {
            Copy-Item -LiteralPath $source -Destination $destination -Force
        }
        $queue.Enqueue($source)
    }
}

$size = (Get-ChildItem $target -Recurse -File | Measure-Object Length -Sum).Sum
Write-Output ("部署完成: {0} (目录大小 {1:N1} MB)" -f $exe, ($size / 1MB))
# 文件用途: 构建脚本, 按配置生成并编译 DrawDesk
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Debug',
    [string]$QtRoot = '',
    [string]$MingwBin = ''
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\Common.ps1"

$root = Split-Path -Parent $PSScriptRoot
$toolchain = Initialize-DrawDeskToolchain -QtRoot $QtRoot -MingwBin $MingwBin
$preset = $Config.ToLower()

Push-Location $root
try {
    & $toolchain.CMake --preset $preset
    if ($LASTEXITCODE -ne 0) { throw '配置失败' }

    & $toolchain.CMake --build --preset $preset
    if ($LASTEXITCODE -ne 0) { throw '构建失败' }

    Write-Output "构建完成: $root\Build\$preset\DrawDesk.exe"

    if (-not (Test-Path (Join-Path $root "Build\$preset\Qt6Core.dll"))) {
        Write-Output "提示: 输出目录缺少 Qt 运行库, 双击运行前请先执行 Scripts\Deploy.ps1"
    }
}
finally {
    Pop-Location
}
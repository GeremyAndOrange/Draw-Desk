# 文件用途: 运行脚本, 以项目内数据目录启动 DrawDesk, 避免写入用户配置目录
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Debug',
    [string]$QtRoot = '',
    [string]$MingwBin = '',
    [switch]$Smoke,
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\Common.ps1"

$root = Split-Path -Parent $PSScriptRoot
$toolchain = Initialize-DrawDeskToolchain -QtRoot $QtRoot -MingwBin $MingwBin
$exe = Join-Path $root "Build\$($Config.ToLower())\DrawDesk.exe"
if (-not (Test-Path $exe)) {
    throw "未找到可执行文件, 请先运行 Build.ps1: $exe"
}

# 数据与缓存全部重定向到项目内, 开发期间不在系统目录产生文件
$env:DRAWDESK_DATA_DIR = Join-Path $root 'Build\DevData'
$env:QML_DISABLE_DISK_CACHE = '1'
$env:QSG_RHI_DISABLE_DISK_CACHE = '1'
if ($Smoke) { $env:DRAWDESK_SMOKE = '1' }
if ($SelfTest) { $env:DRAWDESK_SELFTEST = '1' }

# 使用 .NET 启动, 可靠获取进程对象
# 说明: 若已有实例在运行, 新进程会立即退出, 该情况按正常退出处理
$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = $exe
$startInfo.UseShellExecute = $false

$process = [System.Diagnostics.Process]::Start($startInfo)
if ($null -eq $process) {
    throw "无法启动进程: $exe"
}

$process.WaitForExit()
exit $process.ExitCode
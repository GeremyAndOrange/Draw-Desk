# 文件用途: 打包脚本, 生成便携 ZIP 并调用 Inno Setup 生成安装包
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Release',
    [string]$QtRoot = '',
    [string]$MingwBin = '',
    [string]$InnoCompiler = '',
    [switch]$SkipBuild,
    [switch]$SkipDeploy,
    [switch]$FullQml,
    [switch]$SkipZip
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\Common.ps1"

$root = Split-Path -Parent $PSScriptRoot
$toolchain = Initialize-DrawDeskToolchain -QtRoot $QtRoot -MingwBin $MingwBin
$name = $Config.ToLower()
$buildDir = Join-Path $root "Build\$name"
$payloadParent = Join-Path $root 'Installer\Payload'
$payload = Join-Path $payloadParent 'DrawDesk'
$outputDir = Join-Path $root 'Installer\Output'

$cmakeText = Get-Content (Join-Path $root 'CMakeLists.txt') -Raw -Encoding UTF8
$version = '0.1.0'
if ($cmakeText -match 'VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
    $version = $Matches[1]
}

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'Build.ps1') -Config $Config -QtRoot $toolchain.QtRoot -MingwBin $toolchain.MingwBin
}

if (-not $SkipDeploy) {
    $deployArgs = @{
        Config = $Config
        QtRoot = $toolchain.QtRoot
        MingwBin = $toolchain.MingwBin
    }
    if ($FullQml) { $deployArgs.FullQml = $true }
    & (Join-Path $PSScriptRoot 'Deploy.ps1') @deployArgs
}

$exe = Join-Path $buildDir 'DrawDesk.exe'
if (-not (Test-Path $exe)) {
    throw "未找到可执行文件, 无法打包: $exe"
}

Write-Output '整理打包目录...'
Remove-Item $payload -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory $payload -Force | Out-Null

Copy-Item $exe $payload -Force
Get-ChildItem $buildDir -Filter '*.dll' -File | ForEach-Object { Copy-Item $_.FullName $payload -Force }
foreach ($dir in @('platforms', 'styles', 'imageformats', 'qml')) {
    $source = Join-Path $buildDir $dir
    if (Test-Path $source) {
        Copy-Item $source (Join-Path $payload $dir) -Recurse -Force
    }
}

Copy-Item (Join-Path $root 'Installer\RestoreAll.ps1') $payload -Force
Copy-Item (Join-Path $root 'LICENSE') $payload -Force
Copy-Item (Join-Path $root 'Docs\Manual.md') (Join-Path $payload 'Manual.md') -Force

$licenseTarget = Join-Path $payload 'Licenses'
New-Item -ItemType Directory $licenseTarget -Force | Out-Null
Get-ChildItem (Join-Path $root 'Installer\Licenses') -File | ForEach-Object {
    Copy-Item $_.FullName $licenseTarget -Force
}

$readme = @"
DrawDesk $version 便携版

- 双击 DrawDesk.exe 运行
- 如果程序被强制结束并且无法启动, 以管理员身份运行 RestoreAll.ps1 恢复隐藏窗口
- 配置与日志位于 %APPDATA%\DrawDesk\DrawDesk
- 完整说明见 Manual.md
- 许可证见 LICENSE 与 Licenses 目录
"@
[IO.File]::WriteAllText((Join-Path $payload 'Readme.txt'), $readme, (New-Object Text.UTF8Encoding($false)))

$portableZip = $null
if (-not $SkipZip) {
    New-Item -ItemType Directory $outputDir -Force | Out-Null
    $portableZip = Join-Path $outputDir "DrawDesk-$version-portable.zip"
    Remove-Item $portableZip -Force -ErrorAction SilentlyContinue
    Write-Output '生成便携 ZIP...'
    Compress-Archive -Path $payload -DestinationPath $portableZip -CompressionLevel Optimal -Force
}

function Find-InnoCompiler {
    param([string]$InnoCompiler)

    if ($InnoCompiler) {
        if (Test-Path -LiteralPath $InnoCompiler) {
            return (Resolve-Path -LiteralPath $InnoCompiler).Path
        }
        throw "指定的 Inno Setup 编译器不存在: $InnoCompiler"
    }

    $candidates = New-Object System.Collections.ArrayList
    if ($env:INNO_ISCC) { [void]$candidates.Add($env:INNO_ISCC) }

    if ($env:LOCALAPPDATA) {
        [void]$candidates.AddRange(@(
            (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
            (Join-Path $env:LOCALAPPDATA 'Inno Setup 6\ISCC.exe')
        ))
    }
    if (${env:ProgramFiles(x86)}) {
        [void]$candidates.Add((Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'))
    }
    if ($env:ProgramFiles) {
        [void]$candidates.Add((Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe'))
    }

    $registryKeys = @(
        'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Inno Setup 6_is1',
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Inno Setup 6_is1',
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Inno Setup 6_is1'
    )
    foreach ($key in $registryKeys) {
        try {
            $item = Get-ItemProperty -Path $key -ErrorAction Stop
            if ($item.InstallLocation) {
                [void]$candidates.Add((Join-Path $item.InstallLocation 'ISCC.exe'))
            }
        } catch {
        }
    }

    $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    return $null
}
$installer = Join-Path $outputDir "DrawDeskSetup-$version.exe"
$iscc = Find-InnoCompiler -InnoCompiler $InnoCompiler
if ($iscc) {
    Write-Output "调用 Inno Setup: $iscc"
    Push-Location (Join-Path $root 'Installer')
    try {
        & $iscc "/DAppVersion=$version" 'DrawDesk.iss'
        if ($LASTEXITCODE -ne 0) { throw 'Inno Setup 编译失败' }
    }
    finally {
        Pop-Location
    }
} else {
    Write-Output '未找到 Inno Setup 6, 跳过安装包生成.'
    Write-Output '安装 Inno Setup 6 后重新执行本脚本即可生成安装包, 下载地址: https://jrsoftware.org/isdl.php'
}

if ($portableZip -and (Test-Path $portableZip)) {
    $size = (Get-Item $portableZip).Length / 1MB
    Write-Output ("便携 ZIP: {0} ({1:N1} MB)" -f $portableZip, $size)
}
if ($installer -and (Test-Path $installer)) {
    $size = (Get-Item $installer).Length / 1MB
    Write-Output ("安装包: {0} ({1:N1} MB)" -f $installer, $size)
}
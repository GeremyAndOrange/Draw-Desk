# DrawDesk

> 一个面向 Windows 的窗口与工作区管理工具:把不同任务场景的窗口收进抽屉, 一键切换, 自动恢复布局.

仓库: https://github.com/GeremyAndOrange/Draw-Desk
当前版本: 0.1.1

[产品说明](Docs/Product.md) · [编译与操作手册](Docs/Manual.md) · [详细设计](Docs/Design.md) · [文件说明](Docs/Files.md)

支持 Windows 10 / Windows 11 · C++17 · Qt 6 · QML · MIT License

---

## 功能

- 抽屉管理:添加, 重命名, 删除与排序
- 场景切换:显示当前抽屉窗口, 隐藏其他抽屉窗口
- 布局快照:记录窗口位置, 大小与最大化状态
- 窗口搜索:在主窗口内按进程或标题搜索, 结果可加入当前抽屉
- 每个抽屉独立待办:新增, 编辑, 标记完成与删除
- 加入与移出窗口:支持按钮和全局快捷键
- 规则编辑:进程名, PID, 标题字面匹配或正则匹配
- 每抽屉独立切换快捷键, 前缀固定 `Ctrl+Alt`
- 托盘常驻与一键显示全部
- 本地 JSON 配置, 不联网, 不上传数据
- 独立恢复脚本, 程序无法启动时可以恢复被隐藏窗口

## 下载与安装

从 Releases 页面下载以下任一版本: https://github.com/GeremyAndOrange/Draw-Desk/releases

如果 Releases 暂时没有文件, 可以按下面的从源码构建自行编译, 或本地运行 Scripts\Package.ps1 生成.

- `DrawDesk-<版本>-portable.zip`:解压后双击 `DrawDesk.exe` 直接运行
- `DrawDeskSetup-<版本>.exe`:Inno Setup 安装包, 支持开始菜单, 桌面快捷方式与卸载

程序数据默认保存在 `%APPDATA%\DrawDesk\DrawDesk`, 安装目录保持只读.

## 从源码构建

### 环境要求

- Windows 10 / 11 64 位
- Qt 6.11.2 MinGW 64-bit
- Qt 自带的 MinGW 13.1.0, CMake 3.24 或更高版本
- PowerShell 5.1 或更高版本

脚本会自动查找常见 Qt 安装位置, 也可以显式设置:

```powershell
$env:QT_ROOT  = "C:\Qt\6.11.2\mingw_64"
$env:MINGW_BIN = "C:\Qt\Tools\mingw1310_64\bin"
```

### 构建

```powershell
# Debug
.\Scripts\Build.ps1 -Config Debug

# Release
.\Scripts\Build.ps1 -Config Release

# 复制 Qt 运行库与 QML 模块
.\Scripts\Deploy.ps1 -Config Release

# 运行, 数据写入 Build\DevData
.\Scripts\Run.ps1 -Config Release
```

### 单元测试

```powershell
.\Build\debug\DrawDeskTests.exe
```

或:

```powershell
ctest --test-dir Build\debug --output-on-failure
```

### 打包

```powershell
# 生成便携 ZIP, 并在检测到 Inno Setup 时生成安装包
.\Scripts\Package.ps1 -Config Release
```

输出位于 `Installer\Output`.

## 使用

启动后先打开主窗口, 默认打开五个抽屉:编码, 调试, 文档, 通讯, 其他.

- 左侧管理抽屉, 右侧显示当前抽屉的窗口条目, 位置预览与搜索
- 悬浮窗由托盘菜单显示或隐藏, 仅用于快速切换
- 规则默认按字面标题精确匹配, 需要正则时在规则编辑中打开
- 抓取桌面可以把当前可见应用批量加入选中抽屉

常用快捷键:

| 快捷键 | 作用 |
|---|---|
| `Ctrl+Alt+Z` | 把当前窗口加入活动抽屉 |
| `Ctrl+Alt+X` | 把当前窗口从活动抽屉移出 |
| `Ctrl+Alt+数字/字母/符号` | 切换抽屉, 每个抽屉可单独配置 |
| 托盘左键 | 打开主窗口 |
| 托盘右键 | 显示/隐藏悬浮窗, 退出 |

完整操作说明见 [编译与操作手册](Docs/Manual.md).

## 数据目录

默认路径:`%APPDATA%\DrawDesk\DrawDesk`

- `Config.json`:抽屉, 规则, 布局与快捷键
- `Cloaked.json`:当前被隐藏窗口的记录, 正常退出时删除
- `Logs\`:按天写入的日志

可用环境变量 `DRAWDESK_DATA_DIR` 覆盖.

## 紧急恢复

如果程序被强制结束并且无法启动, 使用安装目录下的恢复脚本:

```powershell
# 默认只恢复 Cloaked.json 记录过的窗口
powershell -ExecutionPolicy Bypass -File .\RestoreAll.ps1

# 强制尝试恢复所有带标题的 Cloak 或不可见窗口
powershell -ExecutionPolicy Bypass -File .\RestoreAll.ps1 -All
```

## 项目结构

```text
DrawDesk
├─ Docs\            产品, 设计, 路线图与操作文档
├─ Installer\        Inno Setup 脚本, 恢复脚本与许可文本
├─ Scripts\          构建, 部署, 运行与打包脚本
├─ Src\              C++ 源码与 QML 界面
│  ├─ Models\        数据结构
│  ├─ Platform\      Win32 与 Qt 平台封装
│  ├─ Services\      抽屉, 规则, 布局与自检
│  └─ Ui\            QML 与图标
└─ Tests\            Qt Test 单元测试
```

## 文档

| 文档 | 内容 |
|---|---|
| [Docs/Product.md](Docs/Product.md) | 产品背景, 理念与功能范围 |
| [Docs/Manual.md](Docs/Manual.md) | 编译, 部署, 操作与恢复 |
| [Docs/Design.md](Docs/Design.md) | 架构, 核心机制与数据模型 |
| [Docs/TechStack.md](Docs/TechStack.md) | 技术选型与许可说明 |
| [Docs/Roadmap.md](Docs/Roadmap.md) | 里程碑与计划 |
| [Docs/Files.md](Docs/Files.md) | 项目文件清单与作用 |

## 许可证

- DrawDesk 自身代码使用 [MIT License](LICENSE)
- Qt 6 以 LGPL v3 动态链接方式使用, 许可文本见 `Installer/Licenses/Qt-License.txt`
- 第三方组件说明见 `Installer/Licenses/ThirdParty.txt`
# 文件说明

说明句:按目录说明 DrawDesk 中每个文件与文件夹的作用, 便于维护与交接.

---

## 一. 根目录

| 文件 | 作用 |
|---|---|
| `README.md` | 产品概念, 功能范围与交互约定 |
| `CMakeLists.txt` | 顶层构建脚本, 定义主程序与测试目标 |
| `CMakePresets.json` | Debug 与 Release 的 CMake 预设, 包含 Qt 与工具链路径 |
| `.gitignore` | 排除构建产物, 日志与本地数据 |
| `.gitattributes` | 统一 LF 行尾 |
| `LICENSE` | DrawDesk 自身代码的 MIT 许可证 |

## 二. Docs

| 文件 | 作用 |
|---|---|
| `Docs/Product.md` | 产品背景, 理念与功能范围 |
| `Docs/TechStack.md` | 技术选型, 界面路线与打包合规 |
| `Docs/Design.md` | 架构, 核心机制, 数据模型与边界 |
| `Docs/Roadmap.md` | 里程碑, 步骤与验收标准 |
| `Docs/Manual.md` | 编译, 部署, 运行, 日常操作与紧急恢复 |
| `Docs/Files.md` | 本文件, 项目文件清单与用途 |

## 三. Src

| 文件 | 作用 |
|---|---|
| `Src/Main.cpp` | 程序入口, 初始化日志, 配置, 单实例, 托盘, 热键与 QML 界面 |
| `Src/Readme.md` | 源码目录结构与构建运行入口 |
| `Src/Models/Drawer.h` | 抽屉, 窗口规则, 布局与待办数据结构 |
| `Src/Platform/Log.h/.cpp` | 日志初始化与数据目录解析 |
| `Src/Platform/ConfigStore.h/.cpp` | 配置读写, 默认配置, 抽屉与快捷键序列化 |
| `Src/Platform/WindowApi.h/.cpp` | 窗口枚举, 规则匹配, DWM 隐藏与显示, 布局读写, 前台跟踪 |
| `Src/Platform/HotkeyService.h/.cpp` | 全局热键注册, 解析与消息分发 |
| `Src/Platform/TrayIcon.h/.cpp` | Win32 Shell_NotifyIcon 托盘图标与菜单 |
| `Src/Platform/SingleInstance.h/.cpp` | 命名互斥体单实例控制 |
| `Src/Services/DrawerService.h/.cpp` | 抽屉切换, 规则匹配, 布局快照, 待办, 隐藏状态与恢复 |
| `Src/Services/SelfTest.h/.cpp` | 启动自检, 使用受控测试窗口验证平台能力 |
| `Src/Ui/Main.qml` | 主窗口, 悬浮窗, 搜索, 规则编辑与设置界面 |
| `Src/Ui/AppIcon.h/.cpp` | 把内置 SVG 渲染为托盘图标句柄 |
| `Src/Ui/AppIcon.svg` | 应用与托盘图标源文件 |
| `Src/Resources/DrawDesk.rc` | Windows 资源脚本, 关联 exe 图标 |
| `Src/Resources/DrawDesk.ico` | Windows 可执行文件图标 |

## 四. Tests

| 文件 | 作用 |
|---|---|
| `Tests/CMakeLists.txt` | 单元测试构建目标与测试注册 |
| `Tests/DrawDeskTests.h/.cpp` | Qt Test 用例: 热键解析, 规则匹配, 配置往返与旧配置迁移 |
| `Tests/Readme.md` | 测试目录说明与运行方式 |
| `Tests/Test.md` | 单元测试执行结果记录 |

## 五. Scripts

| 文件 | 作用 |
|---|---|
| `Scripts/Common.ps1` | 自动解析 Qt 与 MinGW 路径, 支持 `QT_ROOT` 与 `MINGW_BIN` 环境变量 |
| `Scripts/Build.ps1` | 按预设配置并编译 Debug 或 Release |
| `Scripts/Deploy.ps1` | 复制 Qt 运行库, 平台插件与 QML 模块, 依赖 Qt 解析结果 |
| `Scripts/Run.ps1` | 使用项目内数据目录运行程序, 支持冒烟与自检模式, 依赖 Qt 解析结果 |
| `Scripts/Package.ps1` | 一键打包: Release 构建, Deploy, 生成便携 ZIP 并调用 Inno Setup |

## 六. Installer

| 文件 | 作用 |
|---|---|
| `Installer/Readme.md` | 打包阶段说明与恢复脚本入口 |
| `Installer/DrawDesk.iss` | Inno Setup 安装脚本, 生成 Windows 安装包 |
| `Installer/RestoreAll.ps1` | 独立紧急恢复脚本, 程序无法启动时恢复被隐藏窗口 |
| `Installer/Licenses/Qt-License.txt` | Qt LGPL v3 与 GPL v3 许可全文 |
| `Installer/Licenses/ThirdParty.txt` | Qt 与 MinGW 运行库的第三方组件说明 |

# Src

源码目录, 按三层架构组织: 平台层, 服务层, 数据模型与界面层.

---

## 目录

| 目录 | 职责 |
|---|---|
| `Platform/` | Win32 封装与基础能力: 日志, 配置存储, 窗口与托盘 |
| `Services/` | 业务服务: 抽屉切换, 布局快照, 新窗口监听 |
| `Models/` | 数据结构: 抽屉, 窗口规则, 布局快照 |
| `Ui/` | QML 界面文件与界面相关代码 |

## 构建与运行

构建与运行脚本位于 `../Scripts/`, 产物输出到 `../Build/`, 不在系统目录产生文件.

---

- 构建: `../Scripts/Build.ps1 -Config Debug`
- 运行: `../Scripts/Run.ps1 -Config Debug`, 冒烟模式加 `-Smoke`
- 构建系统: CMake 加 MinGW Makefiles; 生成器与编译器路径见 `../CMakePresets.json`
- 完整编译, 操作, 恢复与文件说明: `../Docs/Manual.md` 与 `../Docs/Files.md`

## 当前状态

M4 核心交互:主窗口管理抽屉与窗口条目, 内嵌搜索可直接加入窗口, 悬浮窗快速切换,
规则编辑与每个抽屉独立的快捷键. 平台能力含窗口枚举, DWM 隐藏恢复, 布局快照, 全局热键,
Win32 托盘与单实例互斥. 自动自检 `../Scripts/Run.ps1 -SelfTest` 共 25 项.

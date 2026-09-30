# 编译与操作手册

说明句:给出 DrawDesk 的编译, 部署, 运行, 日常操作与紧急恢复步骤, 供开发机与目标机使用.

---

## 一. 环境要求

- Windows 10 或 Windows 11, 64 位
- Qt 6.11.2 MinGW 64-bit, 可安装在任意目录
- 通过环境变量 `QT_ROOT` 指向 Qt 目录, 通过 `MINGW_BIN` 指向 MinGW 的 bin 目录;不设置时脚本会自动在常见路径中查找
- Qt 自带 CMake 3.30 与 MinGW 13.1.0 工具链
- PowerShell 5.1 或更高版本
- 路径与开发机不同时, 设置 `QT_ROOT` 与 `MINGW_BIN` 即可, 不需要修改项目文件
- 示例: `$env:QT_ROOT = 'C:\Qt\6.11.2\mingw_64'`, `$env:MINGW_BIN = 'C:\Qt\Tools\mingw1310_64\bin'`

## 二. 编译

所有命令在项目根目录 `DrawDesk` 下执行.

---

```powershell
# Debug 构建
.\Scripts\Build.ps1 -Config Debug

# Release 构建
.\Scripts\Build.ps1 -Config Release

# 复制 Qt 运行库与 QML 模块, 生成可独立运行的目录
.\Scripts\Deploy.ps1 -Config Debug

# 运行, 数据写入 Build\DevData, 不污染用户目录
.\Scripts\Run.ps1 -Config Debug

# 冒烟模式, 三秒后自动退出
.\Scripts\Run.ps1 -Config Debug -Smoke

# 应用自检, 启动后自动执行 25 项平台检查并退出
.\Scripts\Run.ps1 -Config Debug -SelfTest
```

- 产物: `Build\debug\DrawDesk.exe` 或 `Build\release\DrawDesk.exe`
- 生成后如果提示缺少 Qt 运行库, 执行一次 `Scripts\Deploy.ps1`
- 单元测试: 构建后执行 `Build\debug\DrawDeskTests.exe`, 或 `ctest --test-dir Build\debug --output-on-failure`

## 三. 打包与发布

本节说明如何生成便携 ZIP 与 Inno Setup 安装包.

---

### 3.1 一键打包

```powershell
# 自动执行 Release 构建, Deploy, 整理 Payload, 生成便携 ZIP 并调用 Inno Setup
.\Scripts\Package.ps1 -Config Release

# Qt 或 MinGW 不在默认路径时追加参数
.\Scripts\Package.ps1 -Config Release -QtRoot "C:\Qt\6.11.2\mingw_64" -MingwBin "C:\Qt\Tools\mingw1310_64\bin"
```

- 便携 ZIP: `Installer\Output\DrawDesk-<版本>-portable.zip`
- 安装包: `Installer\Output\DrawDeskSetup-<版本>.exe`, 需要安装 Inno Setup 6.3 或更高版本
- 打包暂存目录: `Installer\Payload\DrawDesk`, 可以先检查内容再打包
- 环境变量: `QT_ROOT`, `MINGW_BIN`, `QT_CMAKE`
- 常用参数: `-SkipBuild`, `-SkipDeploy`, `-SkipZip`, `-FullQml`, `-InnoCompiler`
- 如果脚本找不到 Inno Setup: `-InnoCompiler "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"`, 或设置环境变量 `INNO_ISCC`

### 3.2 发布到 GitHub Releases

1. 执行 `Scripts\Package.ps1 -Config Release`
2. 在 GitHub 仓库页面打开 Releases, 点击 Draft a new release
3. 标签填 `v0.2.0`, 标题填 `DrawDesk 0.2.0`
4. 上传 `Installer\Output\DrawDesk-0.2.0-portable.zip` 与 `DrawDeskSetup-0.2.0.exe`
5. 发布后 README 的下载入口即可直接使用

### 3.3 安装包说明

- 安装目录默认为 `{autopf}\DrawDesk`, 即 `C:\Program Files\DrawDesk`
- 创建开始菜单快捷方式, 可选创建桌面快捷方式
- 开始菜单附带恢复全部窗口入口
- 卸载时不会删除用户配置, 配置仍在 `%APPDATA%\DrawDesk\DrawDesk`
- 安装包内包含 MIT 许可证, Qt 许可文本与第三方组件说明
## 四. 数据目录

- 默认数据根目录: `%APPDATA%\DrawDesk\DrawDesk`
- 可用环境变量 `DRAWDESK_DATA_DIR` 覆盖, `Run.ps1` 会指向 `Build\DevData`
- `Config.json`: 抽屉, 规则, 布局, 待办与快捷键配置
- `Cloaked.json`: 当前被隐藏窗口的记录, 正常退出时自动删除
- `Logs\`: 按天写入的运行日志

## 五. 日常操作

### 5.1 主窗口与悬浮窗

- 启动程序后打开主窗口; 悬浮窗默认隐藏
- 托盘左键单击打开主窗口, 右键菜单包含显示/隐藏悬浮窗与退出
- 悬浮窗只用于快速切换抽屉, 支持拖动并记忆位置

### 5.2 抽屉管理

- 左侧列表选择抽屉
- 底部输入框加添加按钮新建抽屉
- 操作行提供切换, 改名, 删除, 规则, 设置
- 列表行内的上下箭头调整抽屉顺序
- 每个抽屉可在操作行配置一个切换键位, 与前缀 `Ctrl+Alt` 组合; 留空取消

### 5.3 窗口条目

- 加入窗口: 先把目标窗口切到前台, 再点加入窗口, 或按 `Ctrl+Alt+Z`
- 移出窗口: 列表中选中条目后点移出窗口, 或把目标窗口切到前台后按 `Ctrl+Alt+X`
- 移出按匹配结果执行, 没有匹配时会在底部提示
- 抓取桌面: 把当前可见的应用窗口批量加入选中抽屉
- 刷新: 重新读取窗口列表与位置, 并提示已刷新
- 显示全部: 主窗口左侧抽屉列表下方的按钮, 恢复所有被隐藏窗口

### 5.4 窗口搜索

- 主窗口列表上方输入关键字, 匹配进程名与窗口标题
- 点击结果行会切换到该窗口
- 点击结果行右侧的加入按钮, 把该窗口加入当前选中的抽屉

### 5.5 规则编辑

- 在窗口中选中一条记录后点规则
- 进程名: 精确匹配, 大小写不敏感
- 标题: 默认按字面精确匹配; 点右侧的正则开关后按正则表达式匹配
- PID: 默认填入当前匹配窗口的进程号; 0 或不填表示不启用; 目标程序重启后程序会自动清理失效的进程号
- 保存后立即生效

### 5.6 快捷键

- 全局前缀固定为 `Ctrl+Alt`
- 加入窗口: 默认 `Ctrl+Alt+Z`, 可在设置中修改
- 移出窗口: 默认 `Ctrl+Alt+X`, 可在设置中修改
- 抽屉切换: 每个抽屉单独配置, 可以是数字, 字母或符号
- 保存快捷键时如果组合重复或被其他程序占用, 会给出提示

### 5.7 关闭与退出

- 主窗口右上角关闭: 弹出确认框, 可选择最小化到托盘或退出程序
- 托盘右键退出: 直接退出, 不再弹确认框
- 退出前程序会恢复所有由它隐藏的窗口

### 5.8 抽屉待办

- 右侧面板顶部可切换窗口与待办两个标签
- 待办属于当前选中的抽屉, 切换抽屉时列表随之切换
- 输入内容后按回车或点添加, 即可新增待办
- 点击左侧方框切换完成状态, 已完成项保留在列表下方
- 点击文字可直接编辑, 失焦或按回车保存
- 点击每行右侧的 × 删除单条
- 待办标签上的数字表示该抽屉未完成数量

## 六. 紧急恢复

如果程序被强制结束, 隐藏的窗口会在下次启动后通过显示全部恢复. 如果程序无法启动, 使用安装目录下的恢复脚本.

---

```powershell
# 默认模式: 只恢复 Cloaked.json 中记录过的窗口
powershell -ExecutionPolicy Bypass -File .\RestoreAll.ps1

# 强制模式: 尝试恢复所有带标题的 Cloak 或不可见窗口, 可能影响其他程序
powershell -ExecutionPolicy Bypass -File .\RestoreAll.ps1 -All
```

- 默认数据目录为 `%APPDATA%\DrawDesk\DrawDesk`, 可用 `-DataDir` 指定
- 脚本优先按窗口句柄恢复, 句柄失效时按进程号与标题匹配

## 七. 常见问题

- 提示缺少 `Qt6Core.dll`: 执行 `Scripts\Deploy.ps1`
- 快捷键注册失败: 换一个组合, 通常是与其他程序冲突
- 管理员权限窗口: 隐藏或移动可能失败, 程序只记录并提示, 不强制提权
- 进程号失效: 程序启动与切换抽屉时会自动清理, 规则回退到进程名与标题匹配
- 同进程且标题完全相同的窗口: 外部信息无法区分, 程序可能无法为它们分别保存布局
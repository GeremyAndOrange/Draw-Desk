# Installer

安装包脚本目录, 用于 M5 阶段的打包发布.

---

- 使用 Inno Setup 6.3 或更高版本生成安装包, 脚本为 `DrawDesk.iss`
- 如果 `Package.ps1` 找不到 Inno Setup, 用 `-InnoCompiler` 指定 `ISCC.exe` 的完整路径
- 一键打包入口: `../Scripts/Package.ps1 -Config Release`
- 同时生成便携 ZIP, 不依赖 Inno Setup 也可以发布
- 安装包与 ZIP 内包含 `LICENSE` 与 `Licenses/` 下的 Qt, MinGW 许可说明
- `Package.ps1` 内部会自动执行 Release 构建与 Deploy;Deploy 默认只复制用到的 QML 模块, 完整复制可加 `-FullQml`
- `RestoreAll.ps1` 为紧急恢复脚本, 需随安装包放入安装目录;程序无法启动时执行它可以显示状态文件中记录的隐藏窗口
- 默认只恢复状态文件记录的窗口;加 `-All` 会尝试恢复所有带标题的 Cloak 或隐藏窗口, 可能影响其他程序, 谨慎使用

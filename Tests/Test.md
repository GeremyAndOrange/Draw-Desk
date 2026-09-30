# Test

记录单元测试执行结果.

---

## 2026-09-13 Qt Test 单元测试

- 命令: `Build\debug\DrawDeskTests.exe`
- 结果: 7 通过, 0 失败, 0 跳过
- 用例: `initTestCase`, `hotkeyParsing`, `ruleMatching`, `configRoundTrip`, `todoOperations`,
  `legacyConfigMigration`, `cleanupTestCase`
- 覆盖: 热键解析含数字, 字母, F1 至 F12, 空格与符号键; 规则进程, PID 与字面/正则标题匹配; 配置读写往返含抽屉热键, 待办与窗口尺寸; 旧配置缺少 hotkey 字段时的默认键迁移; 待办新增, 完成, 编辑, 删除与排序逻辑
- 备注: 测试数据目录使用构建目录内 `TestData`, 不写入系统目录
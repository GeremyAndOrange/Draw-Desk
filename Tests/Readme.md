# Tests

测试目录, 存放各模块单元测试, 与 `../Src/` 下的模块对应.

---

## 内容

| 文件 | 说明 |
|---|---|
| `DrawDeskTests.h` / `DrawDeskTests.cpp` | Qt Test 单元测试, 覆盖热键解析, 规则匹配, 配置序列化, 旧配置迁移与待办增删改 |
| `Test.md` | 每次测试执行结果记录 |

## 运行

- 构建: `../Scripts/Build.ps1 -Config Debug`
- 运行: `../Build/debug/DrawDeskTests.exe`
- 或使用 ctest: `ctest --test-dir ../Build/debug --output-on-failure`
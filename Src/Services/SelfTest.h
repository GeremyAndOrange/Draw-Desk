// 文件用途: 平台能力自检接口
#pragma once

namespace DrawDesk::Services {

// 自检结果汇总
struct SelfTestResult {
    int passed = 0;
    int total = 0;

    // 是否全部通过
    bool Ok() const { return total > 0 && passed == total; }
};

// 执行平台层自检: 窗口枚举, 隐藏显示, 单实例与热键状态
SelfTestResult RunPlatformSelfTest(bool trayAvailable, bool windowVisible);

}

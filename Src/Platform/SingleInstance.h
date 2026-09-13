// 文件用途: 单实例接口, 保证同一时间只有一个 DrawDesk 运行
#pragma once

namespace DrawDesk::SingleInstance {

// 获取单实例互斥体, 已有实例时返回 false
bool Acquire();

// 验证互斥体可以被第二个句柄探测到
bool VerifyDetectable();

}

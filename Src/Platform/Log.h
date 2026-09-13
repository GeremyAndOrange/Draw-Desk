// 文件用途: 日志模块接口, 提供初始化与数据目录查询
#pragma once

#include <QString>

namespace DrawDesk::Log {

// 初始化日志系统, 安装 Qt 消息处理器
void Init();

// 返回数据根目录, 环境变量 DRAWDESK_DATA_DIR 优先
QString DataDir();

}

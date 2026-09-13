// 文件用途: 应用图标生成接口, 把内置 SVG 渲染为托盘图标句柄
#pragma once

#include <windows.h>

namespace DrawDesk::Ui {

// 渲染内置 SVG 生成图标句柄, 失败返回空
HICON CreateAppHIcon();

}
